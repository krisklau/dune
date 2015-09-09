//***************************************************************************
// Copyright 2007-2015 Universidade do Porto - Faculdade de Engenharia      *
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
// http://ec.europa.eu/idabc/eupl.html.                                     *
//***************************************************************************
// Author: Artur Zolich                                                     *
//***************************************************************************

// DUNE headers.
#include <DUNE/DUNE.hpp>

namespace RSSI
{
namespace rocket
{
using DUNE_NAMESPACES;


struct Arguments
{
	std::string rocket_data;
	// Rocket data
};


struct Task: public DUNE::Tasks::Periodic
{

	int m_shf0_entity;
	int m_shf1_entity;
	int m_shfB_entity;

	Arguments m_args;


	//! Constructor.
	//! @param[in] name task name.
	//! @param[in] ctx context.
	Task(const std::string& name, Tasks::Context& ctx):
		DUNE::Tasks::Periodic(name, ctx)
	{
		param("Rocket RSSI acquisition Command", m_args.rocket_data)
				.description("System command to fetch Rocket RSSI data");
	}

	//! Update internal state with new parameter values.
	void
	onUpdateParameters(void)
	{
	}

	//! Reserve entity identifiers.
	void
	onEntityReservation(void)
	{
		m_shf0_entity = reserveEntity("SHF - RSSI - CH0");
		m_shf1_entity = reserveEntity("SHF - RSSI - CH1");
		m_shfB_entity = reserveEntity("SHF - RSSI - BOTH");
	}

	//! Resolve entity names.
	void
	onEntityResolution(void)
	{
	}

	//! Acquire resources.
	void
	onResourceAcquisition(void)
	{
	}

	//! Initialize resources.
	void
	onResourceInitialization(void)
	{
	}

	//! Release resources.
	void
	onResourceRelease(void)
	{
	}

	std::string exec(const char* cmd) {
		FILE* pipe = popen(cmd, "r");
		if (!pipe) return "ERROR";
		char buffer[1024];
		std::string result = "";

		while(!feof(pipe)) {
			if(fgets(buffer, 1024, pipe) != NULL){
				result += buffer;
			}
		}
		pclose(pipe);
		return result;
	}


	void
	getRocketRssi(void)
	{
		std::string output = exec(m_args.rocket_data.c_str());

		spew("OUTPUT %s", output.c_str());

		if(output.compare("ERROR")){

			std::string rocket_data_string = output;
			std::size_t foundBegin = rocket_data_string.find("transmitted");
			std::size_t foundEnd = rocket_data_string.find("Antenna profile");


			std::string rocket_rssi_data;


			if ((foundBegin!=std::string::npos) & (foundEnd!=std::string::npos)){
				rocket_rssi_data = rocket_data_string.substr(foundBegin+12, foundEnd -2 - foundBegin-10);

				trace("OUT %s", rocket_rssi_data.c_str());

				std::string rocket_both_rssi_data;
				foundEnd = rocket_rssi_data.find(" rx rssi");
				if ((foundEnd!=std::string::npos)){
					rocket_both_rssi_data = rocket_rssi_data.substr(0, foundEnd);
				}


				std::string rocket_ch0_rssi_data;
				foundBegin = rocket_rssi_data.find("ch0]: ", foundEnd);
				foundEnd = rocket_rssi_data.find('\n', foundBegin);
				if ((foundBegin!=std::string::npos) & (foundEnd!=std::string::npos)){
					rocket_ch0_rssi_data = rocket_rssi_data.substr(foundBegin+6, foundEnd-foundBegin-6);
				}

				std::string rocket_ch1_rssi_data;
				foundBegin = rocket_rssi_data.find("ch1]: ", foundEnd);
				foundEnd = rocket_rssi_data.find('\n', foundBegin);
				if ((foundBegin!=std::string::npos) & (foundEnd!=std::string::npos)){
					rocket_ch1_rssi_data = rocket_rssi_data.substr(foundBegin+6, foundEnd-foundBegin-6);
				}


				IMC::RSSI shf_rssi_IMC_message;
				fp32_t shf_rssi_value;

				//// CHANNEL 0
				shf_rssi_IMC_message.setSourceEntity(m_shf0_entity);

				sscanf(rocket_ch0_rssi_data.c_str(), "%f", &shf_rssi_value);

				shf_rssi_IMC_message.value = shf_rssi_value;
				dispatch(shf_rssi_IMC_message);


				//// CHANNEL 1
				shf_rssi_IMC_message.setSourceEntity(m_shf1_entity);

				sscanf(rocket_ch1_rssi_data.c_str(), "%f", &shf_rssi_value);

				shf_rssi_IMC_message.value = shf_rssi_value;
				dispatch(shf_rssi_IMC_message);


				//// BOTH CHANNELS
				shf_rssi_IMC_message.setSourceEntity(m_shfB_entity);

				sscanf(rocket_both_rssi_data.c_str(), "%f", &shf_rssi_value);

				shf_rssi_IMC_message.value = shf_rssi_value;
				dispatch(shf_rssi_IMC_message);

				debug("Rocket RSSI Histogram: |%s| CH0: |%s| CH1: |%s|", rocket_both_rssi_data.c_str(), rocket_ch0_rssi_data.c_str(), rocket_ch1_rssi_data.c_str());

			}
		}


	}

	//! Main loop.
	void
	task(void)
	{
		getRocketRssi();
	}
};
}
}

DUNE_TASK
