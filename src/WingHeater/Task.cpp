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

namespace WingHeater
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

	int WingHeater_tempOut_entity;
	int WingHeater_humidOut_entity;
	int WingHeater_tempR1_entity;
	int WingHeater_tempR2_entity;
	int WingHeater_tempL1_entity;
	int WingHeater_tempL2_entity;


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
		.defaultValue("9600")
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
		WingHeater_tempOut_entity = reserveEntity("Wing Heater Temp Out");
		WingHeater_humidOut_entity = reserveEntity("Wing Heater Humidity Out");

		WingHeater_tempR1_entity = reserveEntity("Wing Heater Temp Right 1");
		WingHeater_tempR2_entity = reserveEntity("Wing Heater Temp Right 2");
		WingHeater_tempL1_entity = reserveEntity("Wing Heater Temp Left 1");
		WingHeater_tempL2_entity = reserveEntity("Wing Heater Temp Left 2");
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
		IMC::Temperature WingHeater_temp_IMC_message;
		IMC::RelativeHumidity WingHeater_humid_IMC_message;


		float tempL1 =0, tempL2 =0, tempR1 =0, tempR2 =0, tempOut =0, humid =0;

		std::string data = bfr;
		std::size_t foundBegin = data.find("$");
		std::size_t foundEnd = data.find("\n");
		if ((foundBegin != std::string::npos)&&(foundEnd != std::string::npos)){
			data = data.substr(foundBegin, foundEnd-foundBegin);

			spew("|%s|", data.c_str());
			sscanf(data.c_str(), "$%f,%f,%f,%f,%f,%f", &tempL1, &tempL2, &tempR1, &tempR2, &humid, &tempOut);


			WingHeater_temp_IMC_message.setSourceEntity(WingHeater_tempL1_entity);
			WingHeater_temp_IMC_message.value = tempL1;
			dispatch(WingHeater_temp_IMC_message);

			WingHeater_temp_IMC_message.setSourceEntity(WingHeater_tempL2_entity);
			WingHeater_temp_IMC_message.value = tempL2;
			dispatch(WingHeater_temp_IMC_message);


			WingHeater_temp_IMC_message.setSourceEntity(WingHeater_tempR1_entity);
			WingHeater_temp_IMC_message.value = tempR1;
			dispatch(WingHeater_temp_IMC_message);

			WingHeater_temp_IMC_message.setSourceEntity(WingHeater_tempR2_entity);
			WingHeater_temp_IMC_message.value = tempR2;
			dispatch(WingHeater_temp_IMC_message);

			WingHeater_temp_IMC_message.setSourceEntity(WingHeater_tempOut_entity);
			WingHeater_temp_IMC_message.value = tempOut;
			dispatch(WingHeater_temp_IMC_message);

			WingHeater_humid_IMC_message.setSourceEntity(WingHeater_humidOut_entity);
			WingHeater_humid_IMC_message.value = humid;
			dispatch(WingHeater_humid_IMC_message);


			debug("WingHeater TL1: %f, TL2: %f, TR1: %f, TR2: %f, TOut: %f, Humid: %f", tempL1, tempL2, tempR1, tempR2, tempOut, humid);
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

DUNE_TASK
