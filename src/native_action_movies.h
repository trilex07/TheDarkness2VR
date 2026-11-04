#pragma once
#include <cstdint>
#include <cstring>
namespace d2vr_action_movies {
enum : unsigned { Qte=1, Execution=2, Conversation=4, Story=8 };
enum Menu : unsigned { Main=1, Title=2, Options=4, Pause=8, Talents=16 };
inline unsigned ClassifyMenu(const char* name) {
    if(!std::strcmp(name,"MainMenu.swf"))return Main;
    if(!std::strcmp(name,"PressStart.swf"))return Title;
    if(!std::strcmp(name,"Pause.swf") || !std::strcmp(name,"DemoPause.swf"))return Pause;
    if(!std::strcmp(name,"NewTalents.swf") || !std::strcmp(name,"TalentTransition.swf"))return Talents;
    const char* options[]={"Options.swf","OptionsMain.swf","OptionsAudio.swf","OptionsControls.swf",
        "OptionsDisplay.swf","OptionsDisplayCustomize.swf","OptionsGame.swf","CustomizeControls.swf","ControllerLayout.swf"};
    for(auto option:options)if(!std::strcmp(name,option))return Options;
    return 0;
}
inline bool ProtectRapidChoices(unsigned flags) {
    return (flags & (Qte | Execution | Story)) != 0;
}
inline unsigned Classify(const char* name) {
    if(!std::strcmp(name,"QuickTimePopup.swf") || !std::strcmp(name,"QuickTimePopupPersistent.swf"))return Qte;
    if(!std::strcmp(name,"ExecutionPopup.swf"))return Execution;
    if(!std::strcmp(name,"ConversationPopUp.swf"))return Conversation;
    if(!std::strcmp(name,"StoryDialog.swf"))return Story;
    return 0;
}
// Borrow only while the native window/list owner holds movie AND definition
// references. This does not establish ownership. Names are exact asset basenames,
// not substring guesses, and never change cinematic/camera classification.
template<class Read> unsigned InspectDefinition(uint32_t definition,Read read,bool settingsMenu=false) {
    uint32_t table=0,metadata=0,name=0;
    if(definition<0x10000 || (definition&3) || definition>UINT32_MAX-8 ||
       !read(definition,&table,4) || table!=0xFDAFB8 ||
       !read(definition+4,&metadata,4) || metadata<0x10000 || (metadata&3) || metadata>UINT32_MAX-8 ||
       !read(metadata,&table,4) || table!=0xF6B40C || !read(metadata+4,&name,4) ||
       name<0x10000 || name>UINT32_MAX-64)return 0;
    char basename[64]{};
    if(!read(name,basename,sizeof(basename)) || !std::memchr(basename,0,sizeof(basename)))return 0;
    return settingsMenu ? ClassifyMenu(basename) : Classify(basename);
}
}
