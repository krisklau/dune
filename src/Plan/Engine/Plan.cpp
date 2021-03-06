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

// Local headers.
#include "Plan.hpp"

namespace Plan
{
  namespace Engine
  {
    Plan::Plan(const IMC::PlanSpecification* spec, bool compute_progress,
               uint16_t min_cal_time, const SpeedModel* speed_model):
      m_spec(spec),
      m_curr_node(NULL),
      m_sequential(false),
      m_compute_progress(compute_progress),
      m_progress(0.0),
      m_est_cal_time(0),
      m_durations(NULL),
      m_beyond_dur(false),
      m_sched(NULL),
      m_started_maneuver(false),
      m_calib(NULL),
      m_min_cal_time(min_cal_time)
    {
      m_durations = new Plans::Duration(speed_model);
      m_calib = new Calibration();
    }

    Plan::~Plan(void)
    {
      Memory::clear(m_durations);
      Memory::clear(m_sched);
      Memory::clear(m_calib);
    }

    void
    Plan::clear(void)
    {
      m_graph.clear();
      m_seq_nodes.clear();
      m_sequential = false;
      m_durations->clear();
      m_progress = -1.0;
      m_beyond_dur = false;
      m_last_dur = m_durations->end();
      m_started_maneuver = false;
    }

    bool
    Plan::parse(std::string& desc, const std::set<uint16_t>* supported_maneuvers,
                bool plan_startup, const std::map<std::string, IMC::EntityInfo>& cinfo,
                Tasks::Task* task, const IMC::EstimatedState* state)
    {
      bool start_maneuver_ok = false;

      clear();

      if (!m_spec->maneuvers.size())
      {
        desc = m_spec->plan_id + DTR(": no maneuvers");
        return false;
      }

      IMC::MessageList<IMC::PlanManeuver>::const_iterator mitr;
      mitr = m_spec->maneuvers.begin();

      // parse maneuvers and transitions
      do
      {
        if (*mitr == NULL)
        {
          ++mitr;
          continue;
        }

        if ((*mitr)->data.isNull())
        {
          desc = (*mitr)->maneuver_id + DTR(": actual maneuver not specified");
          return false;
        }

        const IMC::Message* m = (*mitr)->data.get();

        if (supported_maneuvers->find(m->getId()) == supported_maneuvers->end())
        {
          desc = (*mitr)->maneuver_id + DTR(": maneuver is not supported");
          return false;
        }

        if ((*mitr)->maneuver_id == m_spec->start_man_id)
          start_maneuver_ok = true;

        Node node;
        bool matched = false;

        node.pman = (*mitr);

        IMC::MessageList<IMC::PlanTransition>::const_iterator tritr;
        tritr = m_spec->transitions.begin();

        for (; tritr != m_spec->transitions.end(); ++tritr)
        {
          if (*tritr == NULL)
            continue;

          if ((*tritr)->dest_man == (*mitr)->maneuver_id)
            matched = true;

          if ((*tritr)->source_man == (*mitr)->maneuver_id)
            node.trans.push_back((*tritr));
        }

        // if a match was not found and this is not the start maneuver
        if (!matched && ((*mitr)->maneuver_id != m_spec->start_man_id))
        {
          std::string str = DTR(": maneuver has no incoming transition"
                                " and it's not the initial maneuver");
          desc = (*mitr)->maneuver_id + str;
          return false;
        }

        m_graph[(*mitr)->maneuver_id] = node;
        ++mitr;
      }
      while (mitr != m_spec->maneuvers.end());

      if (!start_maneuver_ok)
      {
        desc = m_spec->start_man_id + DTR(": invalid start maneuver");
        return false;
      }

      if (m_compute_progress && plan_startup)
      {
        sequenceNodes();

        if (m_sequential && state != NULL)
        {
          computeDurations(state);

          Memory::clear(m_sched);
          m_sched = new ActionSchedule(task, m_spec, m_seq_nodes,
                                       *m_durations, m_last_dur, cinfo);

          // Estimate necessary calibration time
          float diff = m_sched->getEarliestSchedule() - getExecutionDuration();
          m_est_cal_time = (uint16_t)trimValue(diff, 0.0, diff);
          m_est_cal_time = (uint16_t)std::max(m_min_cal_time, m_est_cal_time);
        }
        else if (!m_sequential)
        {
          Memory::clear(m_sched);
          m_sched = new ActionSchedule(task, m_spec, m_seq_nodes, cinfo);

          m_est_cal_time = m_min_cal_time;
        }
      }

      m_last_id = m_spec->start_man_id;

      return true;
    }

    void
    Plan::planStarted(void)
    {
      if (m_sched == NULL)
        return;

      m_affected_ents.clear();

      m_sched->planStarted(m_affected_ents);
    }

    void
    Plan::planStopped(void)
    {
      if (m_sched == NULL)
        return;

      m_sched->planStopped(m_affected_ents);
    }

    void
    Plan::calibrationStarted(void)
    {
      m_calib->setTime(m_est_cal_time);
    }

    void
    Plan::maneuverStarted(const std::string& id)
    {
      m_started_maneuver = true;

      if (m_sched == NULL)
        return;

      m_sched->maneuverStarted(id);
    }

    void
    Plan::maneuverDone(void)
    {
      if (!m_started_maneuver)
        return;

      if (m_curr_node == NULL)
        return;

      if (m_last_dur != m_durations->end())
      {
        if (m_curr_node->pman->maneuver_id == m_last_dur->first)
          m_beyond_dur = true;
      }

      if (m_sched == NULL)
        return;

      m_sched->maneuverDone(m_last_id);
    }

    uint16_t
    Plan::getEstimatedCalibrationTime(void) const
    {
      return m_est_cal_time;
    }

    bool
    Plan::isDone(void) const
    {
      // FIXME: we are only fetching a single transition and not all of them

      if (m_curr_node == NULL)
        return false;
      else if (!m_curr_node->trans.size())
        return true;
      else if (m_curr_node->trans[0]->dest_man == "_done_")
        return true;
      else
        return false;
    }

    IMC::PlanManeuver*
    Plan::loadStartManeuver(void)
    {
      m_last_id = m_spec->start_man_id;

      return loadManeuverFromId(m_last_id);
    }

    IMC::PlanManeuver*
    Plan::loadNextManeuver(void)
    {
      m_last_id = m_curr_node->trans[0]->dest_man;

      return loadManeuverFromId(m_last_id);
    }

    float
    Plan::updateProgress(const IMC::ManeuverControlState* mcs)
    {
      float prog = progress(mcs);

      if (prog >= 0.0 && m_sched != NULL)
      {
        if (!m_beyond_dur)
          m_sched->updateSchedule(getPlanEta());
        else // if we're beyond computed durations, flush all timed actions
          m_sched->flushTimed();
      }

      return prog;
    }

    void
    Plan::updateCalibration(const IMC::VehicleState* vs)
    {
      if (vs->op_mode == IMC::VehicleState::VS_CALIBRATION && m_calib->notStarted())
      {
        m_calib->start();
      }
      else if (vs->op_mode != IMC::VehicleState::VS_CALIBRATION && m_calib->inProgress())
      {
        m_calib->stop();
      }
      else if (m_calib->inProgress())
      {
        // check if some calibration time can be skipped
        if (waitingForDevice())
        {
          m_calib->forceRemainingTime(scheduledTimeLeft());
        }
        else if (m_calib->getElapsedTime() >= m_min_cal_time)
        {
          // If we're past the minimum calibration time
          m_calib->stop();
        }
      }
    }

    bool
    Plan::onEntityActivationState(const std::string& id, const IMC::EntityActivationState* msg)
    {
      if (m_sched != NULL)
        return m_sched->onEntityActivationState(id, msg);
      else
        return true;
    }

    float
    Plan::getPlanEta(void) const
    {
      if (m_progress >= 0.0)
        return getTotalDuration() * (1.0 - 0.01 * m_progress);
      else
        return -1.0;
    }

    // Private

    float
    Plan::getExecutionDuration(void) const
    {
      if (!m_sequential || !m_durations->size())
        return -1.0;

      if (m_last_dur == m_durations->end())
        return -1.0;

      return m_last_dur->second.back();
    }

    bool
    Plan::waitingForDevice(void)
    {
      if (m_sched != NULL)
        return m_sched->waitingForDevice();

      return false;
    }

    float
    Plan::scheduledTimeLeft(void) const
    {
      if (m_sched != NULL)
        return m_sched->calibTimeLeft();

      return -1.0;
    }

    bool
    Plan::maneuverExists(const std::string id) const
    {
      std::vector<IMC::PlanManeuver*>::const_iterator itr = m_seq_nodes.begin();

      for (; itr != m_seq_nodes.end(); ++itr)
        if (!(*itr)->maneuver_id.compare(id))
          return true;

      return false;
    }

    void
    Plan::sequenceNodes(void)
    {
      PlanMap::iterator itr = m_graph.find(m_spec->start_man_id);

      while (true)
      {
        m_seq_nodes.push_back(itr->second.pman);

        if (!itr->second.trans.size())
          break;
        else if (itr->second.trans[0]->dest_man == "_done_")
          break;

        // Check if plan is cyclical
        if (maneuverExists(itr->second.trans[0]->dest_man))
        {
          m_sequential = false;
          return;
        }

        itr = m_graph.find(itr->second.trans[0]->dest_man);
      }

      m_sequential = true;
    }

    void
    Plan::computeDurations(const IMC::EstimatedState* state)
    {
      m_last_dur = m_durations->parse(m_seq_nodes, state);
    }

    IMC::PlanManeuver*
    Plan::loadManeuverFromId(const std::string id)
    {
      PlanMap::iterator itr = m_graph.find(id);

      if (itr == m_graph.end())
      {
        return NULL;
      }
      else
      {
        m_curr_node = &itr->second;
        return m_curr_node->pman;
      }
    }

    float
    Plan::progress(const IMC::ManeuverControlState* mcs)
    {
      if (!m_compute_progress)
        return -1.0;

      // Compute only if sequential and durations exists
      if (!m_sequential || !m_durations->size())
        return -1.0;

      // If calibration has not started yet, but will later
      if (m_calib->notStarted())
        return -1.0;

      float total_duration = getTotalDuration();
      float exec_duration = getExecutionDuration();

      // Check if its calibrating
      if (m_calib->inProgress())
      {
        float time_left = m_calib->getRemaining() + exec_duration;
        m_progress = 100.0 * trimValue(1.0 - time_left / total_duration, 0.0, 1.0);
        return m_progress;
      }

      // If it's not executing, do not compute
      if (mcs->state != IMC::ManeuverControlState::MCS_EXECUTING ||
          mcs->eta == 0)
        return m_progress;

      Duration::ManeuverDuration::const_iterator itr;
      itr = m_durations->find(getCurrentId());

      // If not found
      if (itr == m_durations->end())
      {
        // If beyond the last maneuver with valid duration
        if (m_beyond_dur)
        {
          m_progress = 100.0;
          return m_progress;
        }
        else
        {
          return -1.0;
        }
      }

      // If durations vector for this maneuver is empty
      if (!itr->second.size())
        return m_progress;

      IMC::Message* man = m_graph.find(getCurrentId())->second.pman->data.get();

      // Get execution progress
      float exec_prog = Progress::compute(man, mcs, itr->second, exec_duration);

      float prog = 100.0 - getExecutionPercentage() * (1.0 - exec_prog / 100.0);

      // If negative, then unable to compute
      // But keep last value of progress if it is not invalid
      if (prog < 0.0)
      {
        if (m_progress < 0.0)
          return -1.0;
        else
          return m_progress;
      }

      // Never output shorter than previous
      m_progress = prog > m_progress ? prog : m_progress;

      return m_progress;
    }
  }
}
