#include <Windows.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <iostream>
#include <stdio.h>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <string>
#include <mutex>
#include <thread>
#include <typeinfo>
#include <any>
#include "mini/ini.h"
#include "logger.h"
#include "GeneralFunctions.h"
#include "KeyInput.h"
#include "ConsoleUtil.h"
#include "editorID.hpp"
#include "Serialization.h"
#include "mINIHelper.h"
#include "UIGfx.h"

const std::string consoleCommandEntryPath = "_global.Console.ConsoleInstance.CommandEntry.text";

bool SetConsoleEntryCommand(std::string command, bool execute) {
    logger::info("command[{}] execute[{}]", command, execute);

    if (command == "") {
        logger::error("empty command");
        return false;
    }
    
    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        logger::info("ui not found");
        return false;
    }

    auto menu = ui->GetMenu<RE::Console>();
    if (!menu) {
        logger::info("console menu not found");
        return false;
    }

    auto* msgQueue = RE::UIMessageQueue::GetSingleton();
    if (!msgQueue) {
        logger::info("msgQueue not found");
        return false;
    }

    if (!ui->IsMenuOpen(RE::Console::MENU_NAME)) {
        int i = 0;
        msgQueue->AddMessage(RE::Console::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr); //open console, executes command immediatly
        while (!ui->IsMenuOpen(RE::Console::MENU_NAME) && i < 100) { //wait for console menu to open
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            i++;
        }
    }

    menu->uiMovie->SetVariable(consoleCommandEntryPath.c_str(), command.c_str());
    
    if (execute && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
        RE::GFxKeyEvent enterKeyDown;
        enterKeyDown.keyCode = RE::GFxKey::Code::kKP_Enter;
        enterKeyDown.type = REX::EnumSet<RE::GFxEvent::EventType, std::uint32_t>{ RE::GFxEvent::EventType::kKeyDown };
        enterKeyDown.asciiCode = 28;
        menu->uiMovie->HandleEvent(enterKeyDown);
    }
    return true;
}

void CocOnLoad() {
    mINI::INIFile file("Data/SKSE/Plugins/CocOnLoad.ini");
    mINI::INIStructure ini;
    file.read(ini);
    
    std::string location = mINI::GetIniString(ini, "Main", "sCocLocation", "CocOnLoad_Error");
    if (location == "CocOnLoad_Error") {
        logger::error("Data/SKSE/Plugins/CocOnLoad.ini file or sCocLocation setting not found");
    }
    else if (location == "") {
        logger::info("no coc location");
    }
    else {
        auto* ui = RE::UI::GetSingleton();
        if (ui) {
            bool bAutoExecute = mINI::GetIniBool(ini, "Main", "bAutoExecute", false);
            
            std::string command = "coc " + location;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            while (ui->IsMenuOpen(RE::LoadWaitSpinner::MENU_NAME)) { //wait for main menu to fully load
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            //logger::info("done waiting, load spinner open = {}", ui->IsMenuOpen(RE::LoadWaitSpinner::MENU_NAME));

            SetConsoleEntryCommand(command, bAutoExecute);

            //both of these methods caused ctd
            
            //std::string command = "coc " + location;
            //logger::info("command[{}]", command);
            //ConsoleUtil::ExecuteCommand(command, nullptr);

            //auto* pc = RE::PlayerCharacter::GetSingleton();
            //if (pc) {
            //    logger::info("coc to [{}]", location);
            //    pc->CenterOnCell(location.c_str());
            //}
            //else {
            //    logger::error("pc not found"); //
            //}
        }
    }
}

void MessageListener(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
        // Descriptions are taken from the original skse64 library
        // See:
        // https://github.com/ianpatt/skse64/blob/09f520a2433747f33ae7d7c15b1164ca198932c3/skse64/PluginAPI.h#L193-L212
         //case SKSE::MessagingInterface::kPostLoad: //
            //    logger::info("kPostLoad: sent to registered plugins once all plugins have been loaded");
            //    break;

        //case SKSE::MessagingInterface::kPostPostLoad:
            //    logger::info(
            //        "kPostPostLoad: sent right after kPostLoad to facilitate the correct dispatching/registering of "
            //        "messages/listeners");
            //    break;

        //case SKSE::MessagingInterface::kPreLoadGame:
            //    // message->dataLen: length of file path, data: char* file path of .ess savegame file
            //    logger::info("kPreLoadGame: sent immediately before savegame is read");
            //    break;

        case SKSE::MessagingInterface::kPostLoadGame: {
            // You will probably want to handle this event if your plugin uses a Preload callback
            // as there is a chance that after that callback is invoked the game will encounter an error
            // while loading the saved game (eg. corrupted save) which may require you to reset some of your
            // plugin state.
            //SendLoadGameEvent();
            //CreateEventSinks();
            
            logger::trace("kPostLoadGame: sent after an attempt to load a saved game has finished");
            break;
        }
            //case SKSE::MessagingInterface::kSaveGame:
                //    logger::info("kSaveGame");
                //    break;

            //case SKSE::MessagingInterface::kDeleteGame:
                //    // message->dataLen: length of file path, data: char* file path of .ess savegame file
                //    logger::info("kDeleteGame: sent right before deleting the .skse cosave and the .ess save");
                //    break;

        case SKSE::MessagingInterface::kInputLoaded: {
            logger::info("kInputLoaded: sent right after game input is loaded, right before the main menu initializes");
            
            break;
        }
        case SKSE::MessagingInterface::kNewGame: {
            //logger::trace("kNewGame: sent after a new game is created, before the game has loaded");
            break;
        }
        case SKSE::MessagingInterface::kDataLoaded: {
            // RE::ConsoleLog::GetSingleton()->Print("DbSkseFunctions Installed");
            logger::trace("kDataLoaded: sent after the data handler has loaded all its forms");
            std::thread t(CocOnLoad);
            t.detach();
            break;
        }
    }
}

void LoadCallback(SKSE::SerializationInterface* a_intfc) {
    logger::trace("LoadCallback started");
    std::uint32_t type, version, length;

    if (a_intfc) {
        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            
        }

        // auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton(); 
        // if (vm) {
        //     auto* args = RE::MakeFunctionArguments();
        //     vm->SendEventAll(eventDataPtrs[EventEnum_OnLoadGame]->sEvent, args);
        //     delete args;
        // }

    }
    else {
        logger::error("a_intfc doesn't exist, aborting load.");
    }
}

void SaveCallback(SKSE::SerializationInterface* a_intfc) {
    logger::trace("SaveCallback started");

    if (a_intfc) {
       
    }
    else {
        logger::error("a_intfc doesn't exist, aborting load.");
    }
}

//init================================================================================================================================================================
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SetupLog();

    SKSE::GetMessagingInterface()->RegisterListener(MessageListener);

    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID('SKpi');
    serialization->SetSaveCallback(SaveCallback);
    serialization->SetLoadCallback(LoadCallback);

    //std::stringstream ss;
    //ss << std::hex << std::uppercase << reinterpret_cast<uintptr_t>(SaveCallback);
    //std::string sFunc = ss.str();
    //logger::info("SaveCallback address [{}]", sFunc);
    return true;
}