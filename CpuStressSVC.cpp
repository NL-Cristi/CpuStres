#include <windows.h>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>
#include <direct.h>

// Service name constant used when registering with the Service Control Manager (SCM)
const wchar_t* SERVICE_NAME = L"CpuStressSVC";

SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
SERVICE_STATUS g_ServiceStatus = {0};
std::vector<std::thread> g_WorkerThreads;
std::atomic<bool> g_StopThreads(false);

// Number of worker threads - adjust based on your CPU cores
const int NUM_THREADS = 2;

void Log(const char* message) {
    static std::string logPath;

    // Build the log file path once and cache it
    if (logPath.empty()) {
        char appData[MAX_PATH] = {0};
        DWORD len = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);

        if (len == 0 || len >= MAX_PATH) {
            // Fallback to current directory if APPDATA is not available
            strcpy_s(appData, ".");
        }

        logPath = std::string(appData) + "\\CpuStressSVC";

        // Ensure the directory exists (ignore error if it already exists)
        _mkdir(logPath.c_str());

        logPath += "\\ServiceInfo.log";
    }

    std::ofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        logFile << "[" << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "." << st.wMilliseconds
                << "] " << message << std::endl;
        logFile.close();
    }
}

// CPU-intensive worker function
void CPUIntensiveWork() {
    Log("Worker thread started");

    // Variables for complex calculations
    double result = 0.0;
    int counter = 0;

    while (!g_StopThreads) {
        // Perform CPU-intensive calculations
        for (int i = 0; i < 5000000; i++) {
            // Mix of floating point and integer operations
            result += std::sin(i * 0.0001) * std::cos(i * 0.0002);
            result *= 1.000001;
            result = std::fmod(result, 10.0);

            // Use volatile to prevent compiler optimizations
            volatile int x = i * i;
            counter += x % 17;
        }

        // Add a tiny sleep to prevent 100% CPU usage on one core
        // Adjust this to target ~40% CPU usage
        Sleep(15);  // Sleep for 15ms after each computation batch

        // Log every few cycles
        if (counter % 113 == 0) {
            char buffer[100];
            sprintf_s(buffer, "Worker calculation: %f, Counter: %d", result, counter);
            Log(buffer);
        }
    }

    Log("Worker thread stopped");
}

// Start CPU-intensive threads
void StartCPULoad() {
    Log("Starting CPU load threads");
    g_StopThreads = false;

    for (int i = 0; i < NUM_THREADS; i++) {
        g_WorkerThreads.push_back(std::thread(CPUIntensiveWork));
    }

    Log("CPU load threads started");
}

// Stop CPU-intensive threads
void StopCPULoad() {
    Log("Stopping CPU load threads");
    g_StopThreads = true;

    for (auto& thread : g_WorkerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    g_WorkerThreads.clear();
    Log("CPU load threads stopped");
}

VOID WINAPI ServiceControlHandler(DWORD control) {
    Log("ServiceControlHandler called");

    switch (control) {
        case SERVICE_CONTROL_STOP:
            Log("Received STOP command");
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            g_ServiceStatus.dwControlsAccepted = 0;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

            // Stop CPU load
            StopCPULoad();

            g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            break;
        default:
            Log("Received other command");
            break;
    }

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

// Internal implementation of service main function
VOID WINAPI ServiceMainImpl(DWORD argc, LPWSTR* argv) {
    Log("ServiceMain started");

    // Register the service control handler
    g_StatusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceControlHandler);

    if (!g_StatusHandle) {
        Log("Failed to register control handler");
        return;
    }

    Log("Control handler registered");

    // Set up the service status structure
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 1;
    g_ServiceStatus.dwWaitHint = 1000;

    // Notify SCM of pending start
    if (!SetServiceStatus(g_StatusHandle, &g_ServiceStatus)) {
        Log("Failed to set service status (START_PENDING)");
        return;
    }

    Log("Status set to START_PENDING");

    // Start CPU load
    StartCPULoad();

    // Service has started
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;

    if (!SetServiceStatus(g_StatusHandle, &g_ServiceStatus)) {
        Log("Failed to set service status (RUNNING)");
        return;
    }

    Log("Service started successfully");

    // Keep the service running
    while (g_ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        Sleep(1000);  // Sleep for 1 second
    }
}

// Export with standard service DLL entry points
extern "C" {
    // Standard service entry point - export with different name
    __declspec(dllexport) VOID WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
        Log("Exported ServiceMain called");
        ServiceMainImpl(argc, argv);
    }

    // Alternative name that some versions of Windows expect
    __declspec(dllexport) VOID WINAPI CpuStressSVCServiceMain(DWORD argc, LPWSTR* argv) {
        Log("CpuStressSVCServiceMain called");
        ServiceMainImpl(argc, argv);
    }

    // Another alternative that Windows might look for
    __declspec(dllexport) BOOL WINAPI ServiceEntry(VOID) {
        Log("ServiceEntry called");

        SERVICE_TABLE_ENTRYW ServiceTable[] = {
            { (LPWSTR)SERVICE_NAME, ServiceMainImpl },
            { NULL, NULL }
        };

        BOOL result = StartServiceCtrlDispatcherW(ServiceTable);
        if (!result) {
            Log("StartServiceCtrlDispatcherW failed");
            DWORD error = GetLastError();
            char buffer[100];
            sprintf_s(buffer, "Error code: %lu", error);
            Log(buffer);
        }
        return result;
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        Log("DLL_PROCESS_ATTACH");
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        if (!g_StopThreads) {
            // Make sure threads are stopped if DLL is being unloaded
            g_StopThreads = true;
            StopCPULoad();
        }
        Log("DLL_PROCESS_DETACH");
    }
    return TRUE;
}
