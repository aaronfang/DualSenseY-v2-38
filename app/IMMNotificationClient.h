#include <audiopolicy.h>
#include <endpointvolume.h>
#include <iostream>
#include <mmdeviceapi.h>

class NotificationClient : public IMMNotificationClient
{
public:
    bool DeviceChanged = false;

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId,
                                                   DWORD dwNewState) override
    {
        std::wcout << L"Device state changed: " << pwstrDeviceId
                   << L", New State: " << dwNewState << std::endl;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override
    {
        std::wcout << L"Device added: " << pwstrDeviceId << std::endl;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override
    {
        std::wcout << L"Device removed: " << pwstrDeviceId << std::endl;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE
    OnDefaultDeviceChanged(EDataFlow flow,
                           ERole role,
                           LPCWSTR pwstrDefaultDeviceId) override
    {
        if(pwstrDefaultDeviceId != NULL)
        {
            std::wcout << L"Default device changed: " << pwstrDefaultDeviceId << std::endl;
            DeviceChanged = true;
            return S_OK;
        }
        else {
            return S_FALSE;
        }
    }

    HRESULT STDMETHODCALLTYPE
    OnPropertyValueChanged(LPCWSTR pwstrDeviceId,
                           const PROPERTYKEY key) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE
    OnSessionCreated(IAudioSessionControl *pNewSession)
    {
        return S_OK;
    }

    // Other methods can be left unimplemented for this example
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                             void **ppvObject) override
    {
        if (riid == __uuidof(IUnknown) ||
            riid == __uuidof(IMMNotificationClient))
        {
            *ppvObject = static_cast<IMMNotificationClient *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        return 1;
    }
};
