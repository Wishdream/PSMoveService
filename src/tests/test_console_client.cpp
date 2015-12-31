#include "ClientPSMoveAPI.h"
#include "ClientControllerView.h"
#include <chrono>

#if defined(__linux) || defined (__APPLE__)
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#define FPS_REPORT_DURATION 500 // ms

class PSMoveConsoleClient
{
public:
    PSMoveConsoleClient() 
        : m_keepRunning(true)
        , controller_view(nullptr)
    {
    }

    int run()
    {
        // Attempt to start and run the client
        try 
        {
            if (startup())
            {
                while (m_keepRunning)
                {
                    update();

                    sleep_millisecond(1);
                }
            }
            else
            {
                std::cerr << "Failed to startup the PSMove Client" << std::endl;
            }
        }
        catch (std::exception& e) 
        {
            std::cerr << e.what() << std::endl;
        }

        // Attempt to shutdown the client
        try 
        {
           shutdown();
        }
        catch (std::exception& e) 
        {
            std::cerr << e.what() << std::endl;
        }
      
        return 0;
   }

private:
    void sleep_millisecond(int sleepMs)
    {
    #if defined(__linux) || defined (__APPLE__)
        usleep(sleepMs * 1000);
    #endif
    #ifdef WINDOWS
        Sleep(sleepMs);
    #endif
    }

    // ClientPSMoveAPI
    static void handle_client_psmove_event(
        ClientPSMoveAPI::eClientPSMoveAPIEvent event_type,
        ClientPSMoveAPI::t_event_data_handle opaque_event_handle,
        void *userdata)
    {
        PSMoveConsoleClient *thisPtr= reinterpret_cast<PSMoveConsoleClient *>(userdata);

        switch (event_type)
        {
        case ClientPSMoveAPI::connectedToService:
            std::cout << "PSMoveConsoleClient - Connected to service" << std::endl;

            // Once created, updates will automatically get pushed into this view
            thisPtr->controller_view= ClientPSMoveAPI::allocate_controller_view(0);

            // Kick off request to start streaming data from the first controller
            ClientPSMoveAPI::start_controller_data_stream(
                thisPtr->controller_view, 
                &PSMoveConsoleClient::handle_acquire_controller, thisPtr);
            break;
        case ClientPSMoveAPI::failedToConnectToService:
            std::cout << "PSMoveConsoleClient - Failed to connect to service" << std::endl;
            thisPtr->m_keepRunning= false;
            break;
        case ClientPSMoveAPI::disconnectedFromService:
            std::cout << "PSMoveConsoleClient - Disconnected from service" << std::endl;
            thisPtr->m_keepRunning= false;
            break;
        case ClientPSMoveAPI::opaqueServiceEvent:
            std::cout << "PSMoveConsoleClient - Opaque service event(%d)" << static_cast<int>(event_type) << std::endl;
            thisPtr->m_keepRunning= false;
            break;
        default:
            assert(0 && "Unhandled event type");
            break;
        }
    }

    static void handle_acquire_controller(
        ClientPSMoveAPI::eClientPSMoveResultCode resultCode,
        const ClientPSMoveAPI::t_request_id request_id, 
        ClientPSMoveAPI::t_response_handle opaque_response_handle,
        void *userdata)
    {
        PSMoveConsoleClient *thisPtr= reinterpret_cast<PSMoveConsoleClient *>(userdata);

        if (resultCode == ClientPSMoveAPI::_clientPSMoveResultCode_ok)
        {
            std::cout << "PSMoveConsoleClient - Acquired controller " 
                << thisPtr->controller_view->GetControllerID() << std::endl;

            // Updates will now automatically get pushed into the controller view

            if (thisPtr->controller_view->GetControllerViewType() == ClientControllerView::PSMove)
            {
                const ClientPSMoveView &PSMoveView= thisPtr->controller_view->GetPSMoveView();
                
                if (PSMoveView.GetIsCurrentlyTracking())
                {
                    PSMoveVector3 controller_position= PSMoveView.GetPosition();

                    std::cout << "Controller State: " << std::endl;
                    std::cout << "  Position (" << controller_position.x << ", " << controller_position.y << ", " << controller_position.z << ")" << std::endl;
                }
            }
        }
        else
        {
            std::cout << "PSMoveConsoleClient - failed to acquire controller " << std::endl;
            thisPtr->m_keepRunning= false;
        }
    }

    // PSMoveConsoleClient
    bool startup()
    {
        bool success= true;

        // Attempt to connect to the server
        if (success)
        {
            if (!ClientPSMoveAPI::startup(
                    "localhost", "9512", 
                    &PSMoveConsoleClient::handle_client_psmove_event, this))
            {
                std::cout << "PSMoveConsoleClient - Failed to initialize the client network manager" << std::endl;
                success= false;
            }
        }

        if (success)
        {
            last_report_fps_timestamp= 
                std::chrono::duration_cast< std::chrono::milliseconds >( 
                    std::chrono::system_clock::now().time_since_epoch() );
        }

        return success;
    }

    void update()
    {
        // Process incoming/outgoing networking requests
        ClientPSMoveAPI::update();

        if (controller_view)
        {
            std::chrono::milliseconds now= 
                std::chrono::duration_cast< std::chrono::milliseconds >( 
                    std::chrono::system_clock::now().time_since_epoch() );
            std::chrono::milliseconds diff= now - last_report_fps_timestamp;

            if (diff.count() > FPS_REPORT_DURATION && controller_view->GetDataFrameFPS() > 0)
            {
                std::cout << "PSMoveConsoleClient - DataFrame Update FPS: " << controller_view->GetDataFrameFPS() << "FPS" << std::endl;
                last_report_fps_timestamp= now;
            }
        }
    }

    void shutdown()
    {
        // Free any allocated controller views
        if (controller_view)
        {
            ClientPSMoveAPI::free_controller_view(controller_view);
            controller_view= nullptr;
        }

        // Close all active network connections
        ClientPSMoveAPI::shutdown();
    }

private:
    bool m_keepRunning;
    ClientControllerView *controller_view;
    std::chrono::milliseconds last_report_fps_timestamp;
};

int main(int argc, char *argv[])
{   
    PSMoveConsoleClient app;

    // app instantiation
    return app.run();
}

