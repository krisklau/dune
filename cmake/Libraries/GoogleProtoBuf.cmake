############################################################################
# Copyright 2007-2016 Universidade do Porto - Faculdade de Engenharia      #
# Laboratório de Sistemas e Tecnologia Subaquática (LSTS)                  #
############################################################################
# This file is part of DUNE: Unified Navigation Environment.               #
#                                                                          #
# Commercial Licence Usage                                                 #
# Licencees holding valid commercial DUNE licences may use this file in    #
# accordance with the commercial licence agreement provided with the       #
# Software or, alternatively, in accordance with the terms contained in a  #
# written agreement between you and Universidade do Porto. For licensing   #
# terms, conditions, and further information contact lsts@fe.up.pt.        #
#                                                                          #
# European Union Public Licence - EUPL v.1.1 Usage                         #
# Alternatively, this file may be used under the terms of the EUPL,        #
# Version 1.1 only (the "Licence"), appearing in the file LICENCE.md       #
# included in the packaging of this file. You may not use this work        #
# except in compliance with the Licence. Unless required by applicable     #
# law or agreed to in writing, software distributed under the Licence is   #
# distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF     #
# ANY KIND, either express or implied. See the Licence for the specific    #
# language governing permissions and limitations at                        #
# http://ec.europa.eu/idabc/eupl.html.                                     #
############################################################################
# Author: Kristian Klausen                                                 #
############################################################################

if(PROTOBUF)

  dune_test_header("google/protobuf/message.h")

  if(DUNE_SYS_HAS_GOOGLE_PROTOBUF_MESSAGE_H)
    dune_add_lib(protobuf)

    set(DUNE_SYS_HAS_PROTOBUF 1 CACHE INTERNAL "Protobuf API")
    set(DUNE_USING_PROTOBUF 1 CACHE INTERNAL "Protobuf API")
  else(DUNES_SYS_HAS_GOOGLE_PROTOBUF_MESSAGE_H)
    set(DUNE_SYS_HAS_PROTOBUF 0 CACHE INTERNAL "Protobuf API")
    set(DUNE_USING_PROTOBUF 0 CACHE INTERNAL "Protobuf API")
  endif(DUNE_SYS_HAS_GOOGLE_PROTOBUF_MESSAGE_H)

endif(PROTOBUF)
