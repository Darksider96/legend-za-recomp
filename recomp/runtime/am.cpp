// Applet manager (appletOE) e apm: ciclo de vida da aplicacao, foco, modo de operacao, idioma, desempenho
//
// Numeros de comando e semantica conforme switchbrew (Applet_Manager_services, PPC_services). O jogo roda como se
// estivesse no dock (modo console, desempenho "boost", 1920x1080), com foco e sem parametros de lancamento.

#include "handles.hpp"
#include "recomp/runtime.hpp"
#include "services.hpp"

#include <deque>
#include <mutex>

namespace recomp::ipc {

namespace {

constexpr uint32_t kResultNoMessages = 0x680;      // am 128-3
constexpr uint32_t kResultNoDataInChannel = 0x480; // am 128-2
constexpr uint32_t kMessageFocusStateChanged = 15;
constexpr uint32_t kDockedPerformanceConfiguration = 0x92220008;

// Fila de mensagens do applet: o evento fica sinalizado enquanto houver mensagem
struct AppletState {
    std::mutex mutex;
    std::deque<uint32_t> messages{kMessageFocusStateChanged};
    std::shared_ptr<kernel::Event> message_event = [] {
        auto e = std::make_shared<kernel::Event>();
        e->Signal();
        return e;
    }();
    std::shared_ptr<kernel::Event> generic_event = std::make_shared<kernel::Event>();  // eventos que nunca disparam
};

AppletState& State() {
    static AppletState state;
    return state;
}

TableService::Handler CopyEvent(std::shared_ptr<kernel::Event> event) {
    return [event](Context&, Request&, Response& r) { r.copy_handles.push_back(kernel::Insert(event)); };
}

template <typename T>
TableService::Handler Value(T value) {
    return [value](Context&, Request&, Response& r) { r.Push(value); };
}

std::shared_ptr<Service> CommonStateGetter() {
    return std::make_shared<TableService>(
        "am:ICommonStateGetter",
        std::map<uint32_t, TableService::Handler>{
            {0, CopyEvent(State().message_event)},  // GetEventHandle
            {1, [](Context&, Request&, Response& r) {  // ReceiveMessage
                 AppletState& s = State();
                 std::lock_guard lock(s.mutex);
                 if (s.messages.empty()) {
                     s.message_event->Clear();
                     r.result = kResultNoMessages;
                     return;
                 }
                 r.Push<uint32_t>(s.messages.front());
                 Log("[am] mensagem %u entregue", s.messages.front());
                 s.messages.pop_front();
                 if (s.messages.empty()) s.message_event->Clear();
             }},
            {5, Value<uint8_t>(1)},        // GetOperationMode: console
            {6, Value<uint32_t>(1)},       // GetPerformanceMode: boost
            {8, Value<uint8_t>(0)},        // GetBootMode
            {9, Value<uint8_t>(1)},        // GetCurrentFocusState: em foco
            {10, Accept()},                // RequestToAcquireSleepLock
            {50, Value<uint8_t>(0)},       // IsVrModeEnabled
            {60, [](Context&, Request&, Response& r) {  // GetDefaultDisplayResolution
                 r.Push<int32_t>(1920);
                 r.Push<int32_t>(1080);
             }},
            {61, CopyEvent(State().generic_event)},  // GetDefaultDisplayResolutionChangeEvent
            {66, Accept()},                          // SetCpuBoostRequestPriority
            {90, Value<uint32_t>(kDockedPerformanceConfiguration)},  // GetCurrentPerformanceConfiguration
            {300, Value<uint8_t>(1)},                // GetSettingsPlatformRegion: global
            {7, Value<uint8_t>(1)},                  // GetCradleStatus: no dock
            {55, Value<uint8_t>(0)},                 // IsInControllerFirmwareUpdateSection
            {62, Value<uint32_t>(1)},                // GetHdcpAuthenticationState: autenticado
            {63, CopyEvent(State().generic_event)},  // GetHdcpAuthenticationStateChangeEvent
            {67, Accept()},                          // CancelCpuBoostMode
            {68, Value<uint32_t>(0)},                // GetBuiltInDisplayType
            {100, Accept()},                         // SetHandlingHomeButtonShortPressedEnabled
            {900, Accept()},                         // SetRequestExitToLibraryAppletAtExecuteNextProgramEnabled (16.0+)
        });
}

std::shared_ptr<Service> SelfController() {
    return std::make_shared<TableService>(
        "am:ISelfController",
        std::map<uint32_t, TableService::Handler>{
            {1, Accept()},   // LockExit
            {2, Accept()},   // UnlockExit
            {9, CopyEvent(State().generic_event)},  // GetLibraryAppletLaunchableEvent
            {10, Accept()},  // SetScreenShotPermission
            {11, Accept()},  // SetOperationModeChangedNotification
            {12, Accept()},  // SetPerformanceModeChangedNotification
            {13, Accept()},  // SetFocusHandlingMode
            {14, Accept()},  // SetRestartMessageEnabled
            {16, Accept()},  // SetOutOfFocusSuspendingEnabled
            {19, Accept()},  // SetAlbumImageOrientation
            {40, Value<uint64_t>(1)},  // CreateManagedDisplayLayer -> id da camada
            {50, Accept()},  // SetHandlesRequestToDisplay
            {62, Accept()},  // SetIdleTimeDetectionExtension
            {63, Value<uint32_t>(0)},  // GetIdleTimeDetectionExtension
            {68, Accept()},  // SetAutoSleepDisabled
            {90, Value<uint64_t>(0)},  // GetAccumulatedSuspendedTickValue
            {91, CopyEvent(State().generic_event)},  // GetAccumulatedSuspendedTickChangedEvent
            {100, Accept()}, // SetAlbumImageTakenNotificationEnabled
        });
}

std::shared_ptr<Service> WindowController() {
    return std::make_shared<TableService>("am:IWindowController", std::map<uint32_t, TableService::Handler>{
                                                                      {1, Value<uint64_t>(0x51)},  // GetAppletResourceUserId
                                                                      {10, Accept()},              // AcquireForegroundRights
                                                                  });
}

std::shared_ptr<Service> AudioController() {
    return std::make_shared<TableService>("am:IAudioController", std::map<uint32_t, TableService::Handler>{
                                                                     {0, Accept()},              // SetExpectedMasterVolume
                                                                     {1, Value<float>(1.0f)},    // GetMainAppletExpectedMasterVolume
                                                                     {2, Value<float>(1.0f)},    // GetLibraryAppletExpectedMasterVolume
                                                                     {3, Accept()},              // ChangeMainAppletMasterVolume
                                                                     {4, Accept()},              // SetTransparentVolumeRate
                                                                 });
}

std::shared_ptr<Service> ApplicationFunctions() {
    return std::make_shared<TableService>(
        "am:IApplicationFunctions",
        std::map<uint32_t, TableService::Handler>{
            {1, [](Context&, Request&, Response& r) { r.result = kResultNoDataInChannel; }},  // PopLaunchParameter: nenhum
            {20, Value<uint64_t>(0)},  // EnsureSaveData: nada a ampliar
            {21, Value<uint64_t>(0x00000053552D6E65ull)},  // GetDesiredLanguage: "en-US"
            {22, Accept()},            // SetTerminateResult
            {23, [](Context&, Request&, Response& r) {  // GetDisplayVersion (16 bytes)
                 const char version[16] = "1.0.0";
                 for (char ch : version) r.Push<char>(ch);
             }},
            {40, Value<uint8_t>(1)},   // NotifyRunning
            {50, [](Context&, Request&, Response& r) {  // GetPseudoDeviceId (Uuid)
                 r.Push<uint64_t>(0x5A41524543304D50ull);
                 r.Push<uint64_t>(0x0000000000000001ull);
             }},
            {66, Accept()},            // InitializeGamePlayRecording
            {67, Accept()},            // SetGamePlayRecordingState
            {90, Accept()},            // EnableApplicationCrashReport
            {130, CopyEvent(State().generic_event)},  // GetGpuErrorDetectedSystemEvent
        });
}

// Interfaces que o sdk abre na inicializacao mas que so serao usadas mais tarde (applets, tela, depuracao)
std::shared_ptr<Service> DisplayController() { return std::make_shared<TableService>("am:IDisplayController", std::map<uint32_t, TableService::Handler>{}); }
std::shared_ptr<Service> ProcessWindingController() {
    return std::make_shared<TableService>("am:IProcessWindingController", std::map<uint32_t, TableService::Handler>{});
}
std::shared_ptr<Service> LibraryAppletCreator() {
    return std::make_shared<TableService>("am:ILibraryAppletCreator", std::map<uint32_t, TableService::Handler>{});
}
std::shared_ptr<Service> DebugFunctions() { return std::make_shared<TableService>("am:IDebugFunctions", std::map<uint32_t, TableService::Handler>{}); }

std::shared_ptr<Service> ApplicationProxy() {
    const auto object = [](std::shared_ptr<Service> (*make)()) {
        return [make](Context&, Request&, Response& r) { r.objects.push_back(make()); };
    };
    return std::make_shared<TableService>("am:IApplicationProxy", std::map<uint32_t, TableService::Handler>{
                                                                      {0, object(CommonStateGetter)},
                                                                      {1, object(SelfController)},
                                                                      {2, object(WindowController)},
                                                                      {3, object(AudioController)},
                                                                      {4, object(DisplayController)},
                                                                      {10, object(ProcessWindingController)},
                                                                      {11, object(LibraryAppletCreator)},
                                                                      {20, object(ApplicationFunctions)},
                                                                      {1000, object(DebugFunctions)},
                                                                  });
}

std::shared_ptr<Service> ApmSession() {
    return std::make_shared<TableService>("apm:ISession", std::map<uint32_t, TableService::Handler>{
                                                              {0, Accept()},  // SetPerformanceConfiguration
                                                              {1, Value<uint32_t>(kDockedPerformanceConfiguration)},  // Get...
                                                          });
}

}  // namespace

std::shared_ptr<Service> MakeAppletOE() {
    return std::make_shared<TableService>("appletOE", std::map<uint32_t, TableService::Handler>{
                                                          {0, [](Context&, Request&, Response& r) { r.objects.push_back(ApplicationProxy()); }},
                                                      });
}

std::shared_ptr<Service> MakeApm() {
    return std::make_shared<TableService>("apm", std::map<uint32_t, TableService::Handler>{
                                                     {0, [](Context&, Request&, Response& r) { r.objects.push_back(ApmSession()); }},
                                                     {1, Value<uint32_t>(1)},  // GetPerformanceMode: boost
                                                 });
}

} // namespace recomp::ipc
