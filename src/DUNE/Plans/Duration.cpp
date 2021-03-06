//***************************************************************************
// Copyright 2007-2014 Universidade do Porto - Faculdade de Engenharia      *
// Laboratório de Sistemas e Tecnologia Subaquática (LSTS)                  *
//***************************************************************************
// This file is part of DUNE: Unified Navigation Environment.               *
//                                                                          *
// Commercial Licence Usage                                                 *
// Licencees holding valid commercial DUNE licences may use this file in    *
// accordance with the commercial licence agreement provided with the       *
// Software or, alternatively, in accordance with the terms contained in a  *
// written agreement between you and Universidade do Porto. For licensing   *
// terms, conditions, and further information contact lsts@fe.up.pt.        *
//                                                                          *
// European Union Public Licence - EUPL v.1.1 Usage                         *
// Alternatively, this file may be used under the terms of the EUPL,        *
// Version 1.1 only (the "Licence"), appearing in the file LICENCE.md       *
// included in the packaging of this file. You may not use this work        *
// except in compliance with the Licence. Unless required by applicable     *
// law or agreed to in writing, software distributed under the Licence is   *
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF     *
// ANY KIND, either express or implied. See the Licence for the specific    *
// language governing permissions and limitations at                        *
// https://www.lsts.pt/dune/licence.                                        *
//***************************************************************************
// Author: Pedro Calado                                                     *
//***************************************************************************

// DUNE headers.
#include <DUNE/Plans/Duration.hpp>

namespace DUNE
{
  namespace Plans
  {
    float
    Duration::distance2D(const Position& new_pos, const Position& last_pos)
    {
      return Coordinates::WGS84::distance(new_pos.lat, new_pos.lon, 0.0,
                                          last_pos.lat, last_pos.lon, 0.0);
    }

    float
    Duration::distance3D(const Position& new_pos, const Position& last_pos)
    {
      float value = distance2D(new_pos, last_pos);

      float offset = computeZOffset(new_pos, last_pos);
      float slope = std::atan2(offset, value);
      return value / std::cos(slope);
    }

    template <typename Type>
    float
    Duration::parseSimple(const Type* maneuver, Position& last_pos)
    {
      float speed = convertSpeed(maneuver);

      if (speed == 0.0)
        return -1.0;

      Position pos;
      extractPosition(maneuver, pos);
      float travelled = distance3D(pos, last_pos);

      // compensate with path controller's eta factor
      travelled = compensate(travelled, speed);

      last_pos = pos;

      return travelled / speed;
    }

    template <typename Type>
    float
    Duration::convertSpeed(const Type* maneuver)
    {
      if (m_speed_model == NULL)
        return 0.0;

      float value = SpeedConversion::toMPS(*m_speed_model, maneuver->speed, maneuver->speed_units);

      if (value < 0.0)
        return 0.0;

      return value;
    }

    inline float
    Duration::compensate(float distance, float speed)
    {
      return std::max(0.0f, distance - Control::c_time_factor * speed);
    }

    float
    Duration::computeZOffset(const Position& new_pos, const Position& last_pos)
    {
      if (last_pos.z_units == new_pos.z_units)
      {
        if (last_pos.z_units == IMC::Z_ALTITUDE)
        {
          if (new_pos.binfo.validity && last_pos.binfo.validity)
            return (last_pos.binfo.depth - last_pos.z) - (new_pos.binfo.depth - new_pos.z);
          else
            return last_pos.z - new_pos.z;
        }
        else if (last_pos.z_units == IMC::Z_DEPTH)
        {
          return last_pos.z - new_pos.z;
        }
        else
        {
          return 0.0;
        }
      }
      else
      {
        if (last_pos.z_units == IMC::Z_DEPTH && new_pos.z_units == IMC::Z_ALTITUDE)
        {
          if (new_pos.binfo.validity)
            return last_pos.z - (new_pos.binfo.depth - new_pos.z);
        }
        else if (last_pos.z_units == IMC::Z_ALTITUDE && new_pos.z_units == IMC::Z_DEPTH)
        {
          if (last_pos.binfo.validity)
            return (last_pos.binfo.depth - last_pos.z) - new_pos.z;
        }

        return 0.0;
      }
    }

    void
    Duration::extractBathymetry(const std::string& str, Position& pos)
    {
      (void)str;

      // for now equal to false
      pos.binfo.validity = false;
    }

    template <typename Type>
    void
    Duration::extractPosition(const Type* maneuver, Position& pos)
    {
      pos.lat = maneuver->lat;
      pos.lon = maneuver->lon;
      pos.z = maneuver->z;
      pos.z_units = maneuver->z_units;

      // for now equal to false
      pos.binfo.validity = false;
    }

    void
    Duration::extractPosition(const IMC::EstimatedState* state, Position& pos)
    {
      DUNE::Coordinates::toWGS84(*state, pos.lat, pos.lon);
      pos.z = state->depth;
      pos.z_units = IMC::Z_DEPTH;

      extractBathymetry("", pos);
    }

    void
    Duration::extractPosition(const IMC::Elevator* maneuver, Position& pos)
    {
      pos.lat = maneuver->lat;
      pos.lon = maneuver->lon;
      pos.z = maneuver->start_z;
      pos.z_units = maneuver->start_z_units;

      extractBathymetry("", pos);
    }

    void
    Duration::extractPosition(const IMC::PopUp* maneuver, Position& pos)
    {
      pos.lat = maneuver->lat;
      pos.lon = maneuver->lon;
      pos.z = 0.0;
      pos.z_units = maneuver->z_units;

      extractBathymetry("", pos);
    }

    bool
    Duration::parse(const IMC::FollowPath* maneuver, Position& last_pos)
    {
      float speed = convertSpeed(maneuver);

      if (speed == 0.0)
        return false;

      Position pos;
      pos.z = maneuver->z;
      pos.z_units = maneuver->z_units;

      IMC::MessageList<IMC::PathPoint>::const_iterator itr = maneuver->points.begin();

      if (!maneuver->points.size())
      {
        m_accum_dur->addDuration(0.0);
      }
      else
      {
        // Iterate point list
        for (; itr != maneuver->points.end(); itr++)
        {
          if ((*itr) == NULL)
            continue;

          pos.lat = maneuver->lat;
          pos.lon = maneuver->lon;
          Coordinates::WGS84::displace((*itr)->x, (*itr)->y, &pos.lat, &pos.lon);

          float travelled = distance3D(pos, last_pos);

          last_pos = pos;

          // compensate with path controller's eta factor
          m_accum_dur->addDuration(compensate(travelled, speed) / speed);
        }
      }

      return true;
    }

    bool
    Duration::parse(const IMC::Rows* maneuver, Position& last_pos)
    {
      float speed = convertSpeed(maneuver);

      if (speed == 0.0)
        return false;

      Maneuvers::RowsStages rstages = Maneuvers::RowsStages(maneuver, NULL);

      Position pos;
      pos.z = maneuver->z;
      pos.z_units = maneuver->z_units;

      rstages.getFirstPoint(&pos.lat, &pos.lon);

      float distance = distance3D(pos, last_pos);
      m_accum_dur->addDuration(distance / speed);

      last_pos = pos;

      distance += rstages.getDistance(&last_pos.lat, &last_pos.lon);

      std::vector<float>::const_iterator itr = rstages.getDistancesBegin();

      for (; itr != rstages.getDistancesEnd(); ++itr)
      {
        // compensate with path controller's eta factor
        float travelled = compensate(*itr, speed);
        m_accum_dur->addDuration(travelled / speed);
      }

      return true;
    }

    bool
    Duration::parse(const IMC::YoYo* maneuver, Position& last_pos)
    {
      float speed = convertSpeed(maneuver);

      if (speed == 0.0)
        return false;

      Position pos;
      extractPosition(maneuver, pos);

      // Use 2D distance here
      float horz_dist = distance2D(pos, last_pos);
      float travelled = horz_dist / std::cos(maneuver->pitch);

      // compensate with path controller's eta factor
      travelled = compensate(travelled, speed);

      last_pos = pos;

      m_accum_dur->addDuration(travelled / speed);

      return true;
    }

    bool
    Duration::parse(const IMC::Elevator* maneuver, Position& last_pos)
    {
      float speed = convertSpeed(maneuver);

      if (speed == 0.0)
        return false;

      Position pos;
      extractPosition(maneuver, pos);

      float goto_dist = distance3D(pos, last_pos);
      float amplitude = std::fabs(last_pos.z - maneuver->end_z);
      float real_dist = amplitude / std::sin(c_rated_pitch);

      float travelled = goto_dist + real_dist;

      // compensate with path controller's eta factor
      travelled = compensate(travelled, speed);

      m_accum_dur->addDuration(travelled / speed);

      return true;
    }

    bool
    Duration::parse(const IMC::PopUp* maneuver, Position& last_pos)
    {
      float speed = convertSpeed(maneuver);

      if (speed == 0.0)
        return false;

      // Travel time
      float travel_time;

      if ((maneuver->flags & IMC::PopUp::FLG_CURR_POS) != 0)
      {
        Position pos;
        extractPosition(maneuver, pos);

        float travelled = distance3D(pos, last_pos);

        // compensate with path controller's eta factor
        travelled = compensate(travelled, speed);

        travel_time = travelled / speed;

        last_pos = pos;
      }
      else
      {
        travel_time = 0;
      }

      // Rising time and descending time
      float rising_time;
      float descending_time;
      if (maneuver->z_units == IMC::Z_DEPTH)
      {
        rising_time = (std::fabs(last_pos.z) / std::sin(c_rated_pitch)) / speed;
        descending_time = (std::fabs(maneuver->z) / std::sin(c_rated_pitch)) / speed;
      }
      else // altitude, assume zero
      {
        rising_time = 0.0;
        descending_time = 0.0;
      }

      // surface time
      float surface_time = c_fix_time;

      m_accum_dur->addDuration(travel_time + rising_time + surface_time + descending_time);

      return true;
    }

    Duration::ManeuverDuration::const_iterator
    Duration::parse(const std::vector<IMC::PlanManeuver*>& nodes,
                    const IMC::EstimatedState* state)
    {
      Position pos;
      extractPosition(state, pos);

      std::vector<IMC::PlanManeuver*>::const_iterator itr = nodes.begin();

      for (; itr != nodes.end(); ++itr)
      {
        if ((*itr)->data.isNull())
          return m_durations.end();

        IMC::Message* msg = (*itr)->data.get();

        float last_duration = -1.0;
        if (m_accum_dur != NULL)
        {
          if (m_accum_dur->size())
            last_duration = m_accum_dur->getLastDuration();
        }

        Memory::clear(m_accum_dur);
        m_accum_dur = new AccumulatedDurations(last_duration);

        bool parsed = false;

        switch (msg->getId())
        {
          case DUNE_IMC_GOTO:
            parsed = parse(static_cast<IMC::Goto*>(msg), pos);
            break;

          case DUNE_IMC_STATIONKEEPING:
            parsed = parse(static_cast<IMC::StationKeeping*>(msg), pos);
            break;

          case DUNE_IMC_LOITER:
            parsed = parse(static_cast<IMC::Loiter*>(msg), pos);
            break;

          case DUNE_IMC_FOLLOWPATH:
            parsed = parse(static_cast<IMC::FollowPath*>(msg), pos);
            break;

          case DUNE_IMC_ROWS:
            parsed = parse(static_cast<IMC::Rows*>(msg), pos);
            break;

          case DUNE_IMC_YOYO:
            parsed = parse(static_cast<IMC::YoYo*>(msg), pos);
            break;

          case DUNE_IMC_ELEVATOR:
            parsed = parse(static_cast<IMC::Elevator*>(msg), pos);
            break;

          case DUNE_IMC_POPUP:
            parsed = parse(static_cast<IMC::PopUp*>(msg), pos);
            break;

          case DUNE_IMC_COMPASSCALIBRATION:
            parsed = parse(static_cast<IMC::CompassCalibration*>(msg), pos);
            break;

          default:
            parsed = false;
            break;
        }

        if (!parsed)
        {
          if (m_durations.empty() || itr == nodes.begin())
            return m_durations.end();

          // return the duration from the previously computed maneuver
          ManeuverDuration::const_iterator dtr;
          dtr = m_durations.find((*(--itr))->maneuver_id);

          if (dtr->second.empty())
            return m_durations.end();

          return dtr;
        }

        std::pair<std::string, std::vector<float> > ent((*itr)->maneuver_id,
                                                        m_accum_dur->vec);
        m_durations.insert(ent);
      }

      Memory::clear(m_accum_dur);

      return m_durations.find(nodes.back()->maneuver_id);
    }
  }
}
