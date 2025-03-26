#include <windows.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

// Structure to represent a cursor position
struct CursorPosition {
    int x;
    int y;
};

// Queue for thread-safe position reversion requests
std::queue<CursorPosition> positionQueue;
std::mutex queueMutex;
std::atomic<bool> running(true);

// Mutex for thread-safe logging
std::mutex logMutex;

// Set up logging
std::ofstream logFile("mouse_log.txt");
void log(const std::string &message) {
    std::lock_guard<std::mutex> lock(logMutex);
    // Print to log file
    logFile << message << std::endl;
    logFile.flush(); // Ensure logs are written immediately
    // Print to console
    std::cout << message << std::endl;
}

// MouseMonitor class
class MouseMonitor {
public:
    MouseMonitor(int threshold) : threshold(threshold), previousX(-1), previousY(-1) {}

    void onMove(int x, int y) {
        // Skip processing if we're in the middle of reverting
        if (isReverting) {
            isReverting = false;
            previousX = x;
            previousY = y;
            return;
        }

        if (previousX != -1 && previousY != -1) {
            int deltaX = std::abs(x - previousX);
            int deltaY = std::abs(y - previousY);

            if (deltaX > threshold || deltaY > threshold) {
                log("Jump detected! Delta: (" + std::to_string(deltaX) + ", " +
                    std::to_string(deltaY) + ") Reverting to: (" +
                    std::to_string(previousX) + ", " + std::to_string(previousY) + ")");

                // Queue the reversion to be handled by the UI thread
                requestRevertPosition(previousX, previousY);

                // Don't update previous position when we detect a jump
                return;
            }
        }

        // Update the previous position
        previousX = x;
        previousY = y;
    }

    void requestRevertPosition(int x, int y) {
        CursorPosition pos = {x, y};
        std::lock_guard<std::mutex> lock(queueMutex);
        positionQueue.push(pos);
        isReverting = true;
    }

private:
    int threshold;
    int previousX, previousY;
    bool isReverting = false;
};

// Global monitor instance
MouseMonitor *monitor;

// Thread to process cursor position reversions on the main UI thread
void revertPositionThread() {
    while (running) {
        CursorPosition pos;
        bool hasPos = false;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!positionQueue.empty()) {
                pos = positionQueue.front();
                positionQueue.pop();
                hasPos = true;
            }
        }

        if (hasPos) {
            // Perform the actual cursor reversion
            BOOL result = SetCursorPos(pos.x, pos.y);
            if (result) {
                log("Position successfully reverted to: (" +
                    std::to_string(pos.x) + ", " + std::to_string(pos.y) + ")");
            } else {
                log("Failed to revert position. Error code: " + std::to_string(GetLastError()));
            }

            // Small delay to prevent rapid reversions
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } else {
            // Sleep to reduce CPU usage when idle
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == WM_MOUSEMOVE) {
            MSLLHOOKSTRUCT *pMouseStruct = (MSLLHOOKSTRUCT *)lParam;
            if (pMouseStruct != nullptr) {
                monitor->onMove(pMouseStruct->pt.x, pMouseStruct->pt.y);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void runMouseMonitor() {
    log("Mouse monitor started");

    // Set the threshold for detecting jumps
    monitor = new MouseMonitor(50);

    // Set a low-level mouse hook
    HHOOK mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    if (!mouseHook) {
        log("Failed to set mouse hook. Error code: " + std::to_string(GetLastError()));
        return;
    }

    // Log success
    log("Mouse hook successfully installed");

    // Run a message loop to keep the hook active
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unhook the mouse hook and clean up
    UnhookWindowsHookEx(mouseHook);
    delete monitor;
    log("Mouse monitor stopped");
}

int main() {
    // Open log file
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
        return 1;
    }

    // Start the reversion thread
    std::thread revertThread(revertPositionThread);

    // Start the monitor thread
    std::thread monitorThread(runMouseMonitor);

    // Main program information
    std::cout << "==================================" << std::endl;
    std::cout << "Mouse Monitor Running" << std::endl;
    std::cout << "Log file: mouse_log.txt" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Press Ctrl+C to exit." << std::endl;
    std::cout << std::endl;

    // Create a simple Win32 window (hidden) for processing messages
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("MouseMonitorClass");

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, TEXT("MouseMonitorClass"), TEXT("Mouse Monitor"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), NULL
    );

    // Keep the main program running with message processing
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Signal threads to stop
    running = false;

    // Wait for threads to finish
    if (revertThread.joinable()) {
        revertThread.join();
    }

    if (monitorThread.joinable()) {
        monitorThread.join();
    }

    // Close log file
    logFile.close();

    return 0;
}
