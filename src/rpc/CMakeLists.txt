# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2024 Kdenlive contributors

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  rpc/rpcserver.cpp
  rpc/rpcdispatcher.cpp
  rpc/rpcnotifier.cpp
  rpc/handlers/binhandler.cpp
  rpc/handlers/compositionhandler.cpp
  rpc/handlers/connectionhandler.cpp
  rpc/handlers/projecthandler.cpp
  rpc/handlers/timelinehandler.cpp
  rpc/handlers/transitionhandler.cpp
  PARENT_SCOPE)
