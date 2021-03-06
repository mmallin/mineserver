/*
  Copyright (c) 2010, The Mineserver Project
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the The Mineserver Project nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <deque>
#include <fstream>
#include <vector>
#include <ctime>
#include <math.h>
#include <algorithm>
#include <string>

#ifdef WIN32
  #include <winsock2.h>
#else
  #include <netinet/in.h>
  #include <string.h>
#endif

#include "constants.h"

#include "tools.h"
#include "map.h"
#include "user.h"
#include "chat.h"
#include "config.h"
#include "physics.h"
#include "constants.h"
#include "plugin.h"
#include "mineserver.h"

Chat::Chat()
{
}

Chat::~Chat()
{
}

bool Chat::checkMotd(std::string motdFile)
{
  //
  // Create motdfile is it doesn't exist
  //
  std::ifstream ifs(motdFile.c_str());

  // If file does not exist
  if(ifs.fail())
  {
    Mineserver::get()->screen()->log("> Warning: " + motdFile + " not found. Creating...");

    std::ofstream motdofs(motdFile.c_str());
    motdofs << MOTD_CONTENT << std::endl;
    motdofs.close();
  }

  ifs.close();

  return true;
}

bool Chat::sendUserlist(User* user)
{
  this->sendMsg(user, MC_COLOR_BLUE + "[ " + dtos(User::all().size()) + " players online ]", USER);

  for(unsigned int i = 0; i < User::all().size(); i++)
  {
    std::string playerDesc = "> " + User::all()[i]->nick;
    if(User::all()[i]->muted)
    {
        playerDesc += MC_COLOR_YELLOW + " (muted)";
    }
    if(User::all()[i]->dnd)
    {
      playerDesc += MC_COLOR_YELLOW + " (dnd)";
    }

    this->sendMsg(user, playerDesc, USER);
  }

  return true;
}

std::deque<std::string> Chat::parseCmd(std::string cmd)
{
  int del;
  std::deque<std::string> temp;

  while(cmd.length() > 0)
  {
    while(cmd[0] == ' ')
    {
      cmd = cmd.substr(1);
    }

    del = cmd.find(' ');

    if(del > -1)
    {
      temp.push_back(cmd.substr(0, del));
      cmd = cmd.substr(del+1);
    }
    else
    {
      temp.push_back(cmd);
      break;
    }
  }

  if(temp.empty())
  {
    temp.push_back("empty");
  }

  return temp;
}

bool Chat::handleMsg(User* user, std::string msg)
{
  if (msg.empty()) // If the message is empty handle it as if there is no message.
      return true;

  // Timestamp
  time_t rawTime = time(NULL);
  struct tm* Tm  = localtime(&rawTime);
  std::string timeStamp (asctime(Tm));
  timeStamp = timeStamp.substr(11, 5);

  if ((static_cast<Hook3<bool,User*,time_t,std::string>*>(Mineserver::get()->plugin()->getHook("PlayerChatPre")))->doUntilFalse(user, rawTime, msg))
  {
    return false;
  }
  (static_cast<Hook3<void,User*,time_t,std::string>*>(Mineserver::get()->plugin()->getHook("PlayerChatPost")))->doAll(user, rawTime, msg);
  char prefix = msg[0];

  switch(prefix)
  {
    // Servermsg (Admin-only)
    case SERVERMSGPREFIX:
      if(IS_ADMIN(user->permissions))
      {
        handleServerMsg(user, msg, timeStamp);
      }
      break;

    // Admin message
    case ADMINCHATPREFIX:
      if(IS_ADMIN(user->permissions))
      {
        handleAdminChatMsg(user, msg, timeStamp);
      }
      break;

    // Normal chat message
    default:
      handleChatMsg(user, msg, timeStamp);
      break;
  }

  return true;
}

void Chat::handleServerMsg(User* user, std::string msg, const std::string& timeStamp)
{
  // Decorate server message
  Mineserver::get()->screen()->log(LOG_CHAT, "[!] " + msg.substr(1));
  msg = MC_COLOR_RED + "[!] " + MC_COLOR_GREEN + msg.substr(1);
  this->sendMsg(user, msg, ALL);
}

void Chat::handleAdminChatMsg(User* user, std::string msg, const std::string& timeStamp)
{
  Mineserver::get()->screen()->log(LOG_CHAT, "[@] <"+ user->nick + "> " + msg.substr(1));
  msg = timeStamp +  MC_COLOR_RED + " [@]" + MC_COLOR_WHITE + " <"+ MC_COLOR_DARK_MAGENTA + user->nick + MC_COLOR_WHITE + "> " + msg.substr(1);
  this->sendMsg(user, msg, ADMINS);
}

void Chat::handleChatMsg(User* user, std::string msg, const std::string& timeStamp)
{
  if(user->isAbleToCommunicate("chat") == false)
  {
    return;
  }

  // Check for Admins or Server Console
  if (user->UID == SERVER_CONSOLE_UID)
  {
    Mineserver::get()->screen()->log(LOG_CHAT, user->nick + " " + msg);
    msg = timeStamp + " " + MC_COLOR_RED + user->nick + MC_COLOR_WHITE + " " + msg;
  }
  else if(IS_ADMIN(user->permissions))
  {
    Mineserver::get()->screen()->log(LOG_CHAT, "<"+ user->nick + "> " + msg);
    msg = timeStamp + " <"+ MC_COLOR_DARK_MAGENTA + user->nick + MC_COLOR_WHITE + "> " + msg;
  }
  else
  {
    Mineserver::get()->screen()->log(LOG_CHAT, "<"+ user->nick + "> " + dtos(user->UID) + " " + msg);
    msg = timeStamp + " <"+ user->nick + "> " + msg;
  }

  this->sendMsg(user, msg, ALL);
}

bool Chat::sendMsg(User* user, std::string msg, MessageTarget action)
{
  size_t tmpArrayLen = msg.size()+3;
  uint8* tmpArray    = new uint8[tmpArrayLen];

  tmpArray[0] = 0x03;
  tmpArray[1] = 0;
  tmpArray[2] = msg.size()&0xff;

  for(unsigned int i = 0; i < msg.size(); i++)
  {
    tmpArray[i+3] = msg[i];
  }

  switch(action)
  {
  case ALL:
    user->sendAll(tmpArray, tmpArrayLen);
    break;

  case USER:
     user->buffer.addToWrite(tmpArray, tmpArrayLen);
    break;

  case ADMINS:
    user->sendAdmins(tmpArray, tmpArrayLen);
    break;

  case OPS:
    user->sendOps(tmpArray, tmpArrayLen);
    break;

  case GUESTS:
    user->sendGuests(tmpArray, tmpArrayLen);
    break;

  case OTHERS:
    user->sendOthers(tmpArray, tmpArrayLen);
    break;
  }

  delete[] tmpArray;

  return true;
}
