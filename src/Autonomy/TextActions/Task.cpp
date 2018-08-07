//***************************************************************************
// Copyright 2007-2017 Universidade do Porto - Faculdade de Engenharia      *
// Laboratório de Sistemas e Tecnologia Subaquática (LSTS)                  *
//***************************************************************************
// This file is part of DUNE: Unified Navigation Environment.               *
//                                                                          *
// Commercial Licence Usage                                                 *
// Licencees holding valid commercial DUNE licences may use this file in    *
// accordance with the commercial licence agreement provided with the       *
// Software or, alternatively, in accordance with the terms contained in a  *
// written agreement between you and Faculdade de Engenharia da             *
// Universidade do Porto. For licensing terms, conditions, and further      *
// information contact lsts@fe.up.pt.                                       *
//                                                                          *
// Modified European Union Public Licence - EUPL v.1.1 Usage                *
// Alternatively, this file may be used under the terms of the Modified     *
// EUPL, Version 1.1 only (the "Licence"), appearing in the file LICENCE.md *
// included in the packaging of this file. You may not use this work        *
// except in compliance with the Licence. Unless required by applicable     *
// law or agreed to in writing, software distributed under the Licence is   *
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF     *
// ANY KIND, either express or implied. See the Licence for the specific    *
// language governing permissions and limitations at                        *
// https://github.com/LSTS/dune/blob/master/LICENCE.md and                  *
// http://ec.europa.eu/idabc/eupl.html.                                     *
//***************************************************************************
// Author: Jose Pinto                                                       *
//***************************************************************************

// ISO C++ 98 headers.
#include <stdexcept>

// DUNE headers.
#include <DUNE/DUNE.hpp>

namespace Autonomy
{
  //! This task processes incoming text messages which may
  //! be resulting from received SMSes or Iridium commands.
  namespace TextActions
  {
    using DUNE_NAMESPACES;

    struct Task: public DUNE::Tasks::Task
    {
      //! last received PlanControlState
      IMC::PlanControlState* m_pcs;
      //! last received VehicleState
      IMC::VehicleState* m_vstate;
      //! last received message
      IMC::TextMessage* m_last;
      //! Transmission request id
      int m_reqid;

      //! %Task arguments
      struct Arguments
      {
        //! timeout, in seconds, for replies
        int reply_timeout;
        //! URL for documentation
        std::string help_url;
        //! List of valid commands (does not reply to anything else)
        std::vector<std::string> valid_cmds;

      } m_args;

      Task(const std::string & name, Tasks::Context& ctx):
        DUNE::Tasks::Task(name, ctx),
        m_pcs(NULL),
        m_vstate(NULL),
        m_last(NULL),
        m_reqid(0)
      {
        param("Reply timeout", m_args.reply_timeout)
          .defaultValue("60")
          .minimumValue("30");

        param("Documentation URL", m_args.help_url)
          .defaultValue("https://bit.ly/2LZ0EOc");

        param("Valid Commands", m_args.valid_cmds)
          .defaultValue("abort,dislodge,dive,errors,info,force,go,help,sk,start,surface");

        bind<IMC::TextMessage>(this);
        bind<IMC::VehicleState>(this);
        bind<IMC::PlanControlState>(this);
        bind<IMC::PlanControl>(this);
        bind<IMC::PlanGeneration>(this);
      }

      void
      onResourceInitialization(void)
      {
        setEntityState(IMC::EntityState::ESTA_NORMAL, Status::CODE_ACTIVE);
      }

      void
      consume(const IMC::PlanControlState * msg)
      {
        Memory::replace(m_pcs, msg->clone());
      }

      void
      consume(const IMC::VehicleState * msg)
      {
        Memory::replace(m_vstate, msg->clone());
      }

      void
      consume(const IMC::TextMessage* msg)
      {
        inf("Processing text message from %s: '%s'", msg->origin.c_str(),
            sanitize(msg->text).c_str());
        Memory::replace(m_last, msg->clone());
        std::istringstream iss(msg->text);
        std::string cmd, args;
        splitCommand(msg->text, cmd, args);
        handleCommand(msg->origin, cmd, args);
      }

      //! Handles responses from PlanGeneration requests
      void
      consume(const IMC::PlanGeneration* msg)
      {
        if (m_last == NULL)
          return;

        std::stringstream ss;

        if (msg->op == PlanGeneration::OP_SUCCESS)
        {
          if (msg->params.empty())
            ss << "Started: '" << msg->plan_id << "'.";
          else
            ss << "Started: '" << msg->plan_id << " " << msg->params << "'.";

          reply(m_last->origin, ss.str());
        }
        else if (msg->op == PlanGeneration::OP_ERROR)
        {
          if (std::find(m_args.valid_cmds.begin(), m_args.valid_cmds.end(),
                        msg->plan_id) != m_args.valid_cmds.end())
          {
            ss << "Parser error: '" << msg->params << "'.";
            reply(m_last->origin, ss.str());
          }
        }
      }

      ///! Handles responses from PlanControl requests
      //
      // If the last command failed send that info to the user.
      void
      consume(const IMC::PlanControl* msg)
      {
        if (m_last == NULL)
          return;

        std::string last_cmd, args;
        splitCommand(m_last->text, last_cmd, args);

        // if the last command was a "start" or "force",
        // then the argument is the plan identifier
        if (last_cmd == "start" || last_cmd == "force")
          last_cmd = args;

        if (last_cmd == msg->plan_id)
        {
          // if the last plan request failed then send a reply to the user.
          if (msg->op == PlanControl::PC_START && msg->type == PlanControl::PC_FAILURE)
          {
            std::stringstream ss;
            ss << "Failed to exec " << msg->plan_id <<": " << msg->info << ".";
            reply(m_last->origin, ss.str());
          }
        }
      }

      /**! Splits the text into command and arguments
       * \param[in] text The text to be split
       * \param[out] cmd The first word of the text
       * \param[out] args The remaining text or empty string if text has just one word.
       */
      void
      splitCommand(const std::string& text, std::string& cmd, std::string& args)
      {
        cmd = sanitize(text);
        size_t pos = cmd.find(" ");

        if (pos != std::string::npos)
        {
          args = cmd.substr(pos + 1);
          cmd = cmd.substr(0, pos);
        }

        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (!args.empty())
          inf("Command is '%s', Argument is '%s'", cmd.c_str(), args.c_str());
      }

      //! Send back a reply to a TextMessage's origin
      //! \param origin the original sender
      //! \param text the text to be sent back to the sender
      void
      reply(const std::string& origin, const std::string& text) {
        TransmissionRequest req;

        req.data_mode = TransmissionRequest::DMODE_TEXT;
        req.txt_data = text;
        req.deadline = Clock::getSinceEpoch() + m_args.reply_timeout;
        req.req_id = ++m_reqid;

        // if request was sent over sms
        if (origin.find("+") == 0)
        {
          req.comm_mean = TransmissionRequest::CMEAN_GSM;
          req.destination = origin;
          inf("Replying to %s: '%s'", req.destination.c_str(), req.txt_data.c_str());
          dispatch(req);
        }
        else if (origin == "iridium")
        {
          req.comm_mean = TransmissionRequest::CMEAN_SATELLITE;
          req.destination = "";
          inf("Replying via Iridium: '%s'", req.txt_data.c_str());
          dispatch(req);
        }
        else
          war("Not replying as origin is not addressable: '%s'.",
              origin.c_str());
      }

      //! Parse incoming command and run it.
      //! \param origin The original sender
      //! \param cmd The command to be executed
      //! \param args The arguments of the command (if any)
      void
      handleCommand(const std::string& origin, const std::string& cmd, const std::string& args)
      {
        if (cmd == "start")
          handleStartCommand(origin, args, false);
        else if (cmd == "force")
          handleStartCommand(origin, args, true);
        else if (cmd == "abort")
          handleAbortCommand(origin, args);
        else if (cmd == "errors")
          handleErrorsCommand(origin);
        else if (cmd == "info")
          handleInfoCommand(origin);
        else if (cmd == "help")
          handleHelpCommand(origin);
        else
          handlePlanGeneratorCommand(origin, cmd, args);
      }

      //! Execute command 'ERRORS'
      void
      handleErrorsCommand(const std::string& origin)
      {
        std::stringstream ss;

        if (m_vstate != NULL)
        {
          if (m_vstate->error_count > 0) {
            ss << (int) m_vstate->error_count << " errors: " << m_vstate->error_ents;
            reply(origin, ss.str());
          }
          else
            reply(origin, "Vehicle has no reported errors.");
        }
      }

      //! Execute command 'INFO'
      void
      handleInfoCommand(const std::string& origin)
      {
        std::stringstream ss;
        if (m_pcs == NULL || m_vstate == NULL)
        {
          war("Not replying as no state messages are available.");
          return;
        }

        bool executing = m_pcs->state == PlanControlState::PCS_EXECUTING;
        bool initializing = m_pcs->state == PlanControlState::PCS_INITIALIZING;
        bool succeeded = m_pcs->last_outcome == PlanControlState::LPO_SUCCESS &&
            (m_pcs->state == PlanControlState::PCS_READY);
        bool ready = m_pcs->last_outcome == PlanControlState::LPO_NONE &&
            (m_pcs->state == PlanControlState::PCS_READY);

        if (executing)
        {
          ss << "Executing " << m_pcs->plan_id << "::" << m_pcs->man_id << " | ETA: ";

          if (m_pcs->plan_eta != -1)
            ss << m_pcs->plan_eta << "s / " << std::fixed << std::setprecision(1) << m_pcs->plan_progress << "%.";
          else
            ss << "N/D.";
        }
        else if (initializing)
          ss << "Initializing " << m_pcs->plan_id << ".";
        else if (ready)
          ss << "Vehicle is ready.";
        else if (succeeded)
          ss << "Finished " << m_pcs->plan_id << ".";
        else
          ss << "Failed to exec " << m_pcs->plan_id <<": " << m_vstate->last_error << ".";

        reply(origin, ss.str());
      }

      //! Execute command 'START'
      void
      handleStartCommand(const std::string& origin, const std::string& args, bool ignore_errors = true)
      {
        // Plan control message!
        IMC::PlanControl pc;
        pc.type = IMC::PlanControl::PC_REQUEST;
        pc.op = IMC::PlanControl::PC_START;
        if (ignore_errors)
          pc.flags = IMC::PlanControl::FLG_IGNORE_ERRORS;

        char plan_id[32];
        std::sscanf(args.c_str(), "%s", plan_id);
        pc.plan_id = plan_id;

        inf(DTR("received SMS request to start plan '%s'"),
            sanitize(pc.plan_id).c_str());
        std::stringstream ss;
        ss << "Started execution of " << plan_id;
        if (ignore_errors)
          ss << " ignoring errors.";
        else
          ss << ".";
        reply(origin, ss.str());

        // Send the plan start request
        dispatch(pc);
        (void)origin;
      }

      //! Execute command 'ABORT'
      void
      handleAbortCommand(const std::string& origin, const std::string& args)
      {
        err(DTR("got abort request from '%s'"), origin.c_str());
        IMC::Abort abort;
        abort.setDestination(getSystemId());
        dispatch(abort);
        reply(origin, "Aborted.");
        (void)args;
      }

      //! Execute command 'HELP'
      void
      handleHelpCommand(const std::string& origin)
      {
        std::stringstream ss;
        ss << "For a list of valid commands see " << m_args.help_url << ".";
        reply(origin, ss.str());
      }

      //! Sends a PlanGeneration request resulting from an unknown command.
      void
      handlePlanGeneratorCommand(const std::string& origin, const std::string& cmd, const std::string& args)
      {
        IMC::PlanGeneration pg;

        pg.cmd = IMC::PlanGeneration::CMD_EXECUTE;
        pg.op = IMC::PlanGeneration::OP_REQUEST;
        pg.plan_id = cmd;
        pg.params = args;
        dispatch(pg);
        (void)origin;
      }

      void
      onMain(void)
      {
        while (!stopping())
        {
          waitForMessages(1.0);
        }
      }
    };
  }
}

DUNE_TASK
