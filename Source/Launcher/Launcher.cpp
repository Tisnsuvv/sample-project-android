#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/IO/FileSystem.h>

#include <Player/PlayerApplication.h>

#if MOBILE
#include <SDL.h>
#endif

using namespace Urho3D;

class LauncherApplication : public PlayerApplication
{
public:
    using PlayerApplication::PlayerApplication;

    void Setup() override
    {
        PlayerApplication::Setup();

#if MOBILE
        engineParameters_[EP_ORIENTATIONS] = "Portrait";
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";..;../..";
#else
        auto fs = context_->GetSubsystem<FileSystem>();
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = fs->GetProgramDir() + ";" + fs->GetCurrentDir() + "../";
#endif

        engineParameters_[EP_PLUGINS] = ea::string::joined(LinkedPlugins::GetLinkedPlugins(), ";");
        LinkedPlugins::RegisterStaticPlugins();
    }

    void Start() override
    {
        PlayerApplication::Start();

#if MOBILE
        // The engine disables SDL's touch->mouse synthesis because it handles
        // touches natively. The UI of this sample is desktop-oriented (RmlUi
        // uses the mouse), so re-enable it to make the app usable on phones.
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
#endif
    }
};

URHO3D_DEFINE_APPLICATION_MAIN(LauncherApplication);
