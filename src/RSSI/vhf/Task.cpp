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

// ISO C++ 98 headers.
#include <cstring>
#include <cstddef>
#include <string>

// DUNE headers.
#include <DUNE/DUNE.hpp>

namespace RSSI
{
namespace vhf
{
using DUNE_NAMESPACES;


struct Arguments
{
	// Serial port device.
	std::string uart_dev;
	// Serial port baud rate.
	unsigned uart_baud;

};

struct Task: public DUNE::Tasks::Periodic
{

	int m_vhf_entity;


	// Device protocol handler.
	SerialPort* m_uart;
	// Task Arguments.
	Arguments m_args;

	// I/O Multiplexer.
	Poll m_poll;

	//! Constructor.
	//! @param[in] name task name.
	//! @param[in] ctx context.
	Task(const std::string& name, Tasks::Context& ctx):
		DUNE::Tasks::Periodic(name, ctx)
	{
		param("Serial Port - Device", m_args.uart_dev)
        								  .defaultValue("/dev/ttyUSB0")
        								  .description("Serial port device (used to communicate with the actuator)");

		param("Serial Port - Baud Rate", m_args.uart_baud)
		.defaultValue("38400")
		.description("Serial port baud rate");

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
		m_vhf_entity = reserveEntity("VHF - RSSI");

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
		m_uart = new SerialPort(m_args.uart_dev, m_args.uart_baud);

	}

	//! Initialize resources.
	void
	onResourceInitialization(void)
	{

		m_poll.add(*m_uart);
	}

	//! Release resources.
	void
	onResourceRelease(void)
	{
		if (m_uart != NULL)
		{
			m_poll.remove(*m_uart);
			delete m_uart;
			m_uart = NULL;
		}

	}

	void
	process(const char* bfr)
	{
		IMC::RSSI vhf_rssi_IMC_message;
		vhf_rssi_IMC_message.setSourceEntity(m_vhf_entity);
		//			getEntityId();

		std::string vhf_data_string = bfr;

		std::string vhf_rssi_text;


		std::size_t oldDBM;
		std::size_t foundRX = vhf_data_string.find("RX ", oldDBM);
		std::size_t foundDBM = vhf_data_string.find("dBm", foundRX);
		oldDBM = foundDBM;

		spew("%s", vhf_data_string.c_str());

		if ((foundRX!=std::string::npos) & (foundDBM!=std::string::npos)){
			vhf_rssi_text = vhf_data_string.substr(foundRX+3, foundDBM-4 - foundRX+1);

			fp32_t vhf_rssi_value;
			sscanf(vhf_rssi_text.c_str(), "%f", &vhf_rssi_value);

			debug("VHF RSSI: |%s|", vhf_rssi_text.c_str());

			vhf_rssi_IMC_message.value = vhf_rssi_value;
			dispatch(vhf_rssi_IMC_message);
			foundRX = vhf_data_string.find("RX ", oldDBM);
			foundDBM = vhf_data_string.find("dBm", foundRX);
			oldDBM = foundDBM;
		}
	}

	void
	checkSerialPort(void)
	{
		if (m_poll.wasTriggered(*m_uart))
		{
			char bfr[1024];
			int rv = m_uart->read(bfr, sizeof(bfr));
			if(rv > 0){
				process(bfr);
			}
		}
	}




	//! Main loop.
	void
	task(void)
	{
		if (m_poll.poll(0.1))
		{
			checkSerialPort();

		}
	}
};
}
}

DUNE_TASK
