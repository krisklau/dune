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

#ifndef DUNE_PLAN_ENGINE_PLAN_HPP_INCLUDED_
#define DUNE_PLAN_ENGINE_PLAN_HPP_INCLUDED_

// ISO C++ 98 headers.
#include <map>
#include <string>
#include <vector>

// DUNE headers.
#include <DUNE/Plans.hpp>
#include "Calibration.hpp"
#include "ActionSchedule.hpp"

namespace Plan
{
  namespace Engine
  {
    using namespace DUNE::Plans;

    // Export DLL Symbol.
    class DUNE_DLL_SYM Plan;

    //! Plan Specification parser
    class Plan
    {
    public:
      //! Default constructor.
      //! @param[in] spec pointer to PlanSpecification message
      //! @param[in] compute_progress true if progress should be computed
      //! @param[in] min_cal_time minimum calibration time in s.
      //! @param[in] speed_model pointer to model for speed conversions
      Plan(const IMC::PlanSpecification* spec, bool compute_progress,
           uint16_t min_cal_time, const SpeedModel* speed_model);

      //! Destructor
      ~Plan(void);

      //! Reset data
      void
      clear(void);

      //! Parse a given plan
      //! @param[out] desc description of the failure if any
      //! @param[in] supported_maneuvers list of supported maneuvers
      //! @param[in] plan_startup true if the plan is starting up
      //! @param[in] cinfo map of components info
      //! @param[in] task pointer to task
      //! @param[in] state pointer to EstimatedState message
      //! @return true if was able to parse the plan
      bool
      parse(std::string& desc, const std::set<uint16_t>* supported_maneuvers,
            bool plan_startup, const std::map<std::string, IMC::EntityInfo>& cinfo,
            Tasks::Task* task, const IMC::EstimatedState* state = NULL);

      //! Signal that the plan has started
      void
      planStarted(void);

      //! Signal that the plan has stopped
      void
      planStopped(void);

      //! Signal that calibration has started
      void
      calibrationStarted(void);

      //! Signal that a maneuver has started
      //! @param[in] id name of the started maneuver
      void
      maneuverStarted(const std::string& id);

      //! Signal that current maneuver is done
      void
      maneuverDone(void);

      //! Get necessary calibration time
      //! @return necessary calibration time
      uint16_t
      getEstimatedCalibrationTime(void) const;

      //! Check if plan has been completed
      //! @return true if plan is done
      bool
      isDone(void) const;

      //! Get start maneuver message
      //! @return NULL if start maneuver id is invalid
      IMC::PlanManeuver*
      loadStartManeuver(void);

      //! Get next maneuver message
      //! @return NULL if maneuver id is invalid
      IMC::PlanManeuver*
      loadNextManeuver(void);

      //! Get current maneuver id
      //! @return current id string
      inline std::string
      getCurrentId(void) const
      {
        return m_last_id;
      }

      //! Get calibration info string
      //! @return calibration info string
      inline const std::string
      getCalibrationInfo(void) const
      {
        return m_calib->getInfo();
      }

      //! Is calibration done
      //! @return true if so, false otherwise
      inline bool
      isCalibrationDone(void) const
      {
        return m_calib->isDone();
      }

      //! Has calibration failed
      //! @return true if so, false otherwise
      inline bool
      hasCalibrationFailed(void) const
      {
        return m_calib->hasFailed();
      }

      //! Get current plan progress
      //! @param[in] mcs pointer to maneuver control state message
      //! @return progress in percent (-1.0 if unable to compute)
      float
      updateProgress(const IMC::ManeuverControlState* mcs);

      //! Update calibration process
      void
      updateCalibration(const IMC::VehicleState* vs);

      //! Pass EntityActivationState to scheduler
      //! @param[in] id entity label
      //! @param[in] msg pointer to EntityActivationState message
      //! @return false if something failed to be activated, true otherwise
      bool
      onEntityActivationState(const std::string& id, const IMC::EntityActivationState* msg);

      //! Get plan estimated time of arrival
      //! @return ETA
      float
      getPlanEta(void) const;

    private:
      //! Get duration of the execution phase of the plan
      //! (total of maneuver accumulated duration)
      //! @return duration of the execution phase of the plan
      float
      getExecutionDuration(void) const;

      //! Get total duration of the plan
      //! @return total duration of the plan
      inline float
      getTotalDuration(void) const
      {
        return getExecutionDuration() + getEstimatedCalibrationTime();
      }

      //! Get execution percentage
      //! @return percentage of the plan represented by the execution
      inline float
      getExecutionPercentage(void) const
      {
        return getExecutionDuration() / getTotalDuration() * 100.0;
      }

      //! Check if scheduler is waiting for a device
      //! @return true if waiting for device
      bool
      waitingForDevice(void);

      //! Returns calibration time left according to scheduler
      //! @return calibration time left or -1 if no scheduler is active
      float
      scheduledTimeLeft(void) const;

      //! Check if a maneuver exists in the sequential nodes
      //! @param[in] id string id of the maneuver
      //! @return true if it already exists in the seq nodes vector
      bool
      maneuverExists(const std::string id) const;

      //! Sequence plan nodes if possible
      void
      sequenceNodes(void);

      //! Compute durations of each point in the plan
      //! @param[in] pointer to estimated state message
      void
      computeDurations(const IMC::EstimatedState* state);

      //! Get maneuver from id
      //! @param[in] id name of the maneuver to load
      //! @return NULL if maneuver id is invalid
      IMC::PlanManeuver*
      loadManeuverFromId(const std::string id);

      //! Compute current progress
      //! @param[in] pointer to ManeuverControlState message
      //! @return progress in percent (-1.0 if unable to compute)
      float
      progress(const IMC::ManeuverControlState* mcs);

      //! Graph nodes (a maneuver and its outgoing transitions)
      struct Node
      {
        //! Pointer to a plan maneuver
        IMC::PlanManeuver* pman;
        //! Vector of pointers to plan transitions
        std::vector<IMC::PlanTransition*> trans;
      };

      //! Mapping between maneuver IDs and graph nodes
      typedef std::map<std::string, Node> PlanMap;

      //! Pointer to plan specification
      const IMC::PlanSpecification* m_spec;
      //! Plan graph of maneuvers and transitions
      PlanMap m_graph;
      //! Pointer to current node
      Node* m_curr_node;
      //! Last maneuver id
      std::string m_last_id;
      //! True if plan is sequential
      bool m_sequential;
      //! Whether or not to compute plan's progress
      bool m_compute_progress;
      //! Current progress if any
      float m_progress;
      //! Estimated required calibration time
      uint16_t m_est_cal_time;
      //! Vector of message pointers to cycle through (sequential) plan
      std::vector<IMC::PlanManeuver*> m_seq_nodes;
      //! Pointer to maneuver durations
      Plans::Duration* m_durations;
      //! Iterator to last maneuver with a valid duration
      Duration::ManeuverDuration::const_iterator m_last_dur;
      //! Flag to signal that the plan is past the last maneuver with a valid duration
      bool m_beyond_dur;
      //! Schedule for actions to take during plan
      ActionSchedule* m_sched;
      //! Vector of entity labels to push and pop entity parameters
      std::vector<std::string> m_affected_ents;
      //! Signal that a maneuver has started
      bool m_started_maneuver;
      //! Calibration object pointer
      Calibration* m_calib;
      //! Minimum calibration time
      uint16_t m_min_cal_time;
    };
  }
}

#endif
