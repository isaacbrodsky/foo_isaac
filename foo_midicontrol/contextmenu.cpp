#include "stdafx.h"

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// Identifier of our context menu group. Substitute with your own when reusing code.
// {9B5CEBA1-A23C-475B-AE39-77AF017D04CA}
static const GUID g_mainmenu_group_id = { 0x9b5ceba1, 0xa23c, 0x475b,{ 0xae, 0x39, 0x77, 0xaf, 0x1, 0x7d, 0x4, 0xca } };
// {7E6AF0FB-75EB-487D-B71C-ECCE08A7A194}
static const GUID g_mainmenu_group_out_id = { 0x7e6af0fb, 0x75eb, 0x487d,{ 0xb7, 0x1c, 0xec, 0xce, 0x8, 0xa7, 0xa1, 0x94 } };

static mainmenu_group_popup_factory g_mainmenu_group(g_mainmenu_group_id, mainmenu_groups::file, mainmenu_commands::sort_priority_dontcare, "Enable MIDI control");
static mainmenu_group_popup_factory g_mainmenu_group_out(g_mainmenu_group_out_id, mainmenu_groups::file, mainmenu_commands::sort_priority_dontcare, "Enable MIDI output");

static t_uint32 selectedMidiInDevice = -1;
static t_uint32 selectedMidiOutDevice = -1;
static HMIDIIN hMidiDevice = NULL;
static HMIDIOUT hMidiOutDevice = NULL;

struct midi_control_callback : main_thread_callback {
private:
    UINT wMsg;
    DWORD dwInstance;
    DWORD dwParam1;
    DWORD dwParam2;

public:
    midi_control_callback(UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
    : wMsg(wMsg), dwInstance(dwInstance), dwParam1(dwParam1), dwParam2(dwParam2) {
    }

    void callback_run() override {
        static_api_ptr_t<playback_control> m_playback_control;

        /*
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=007F5B90, dwParam2=00036D07
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=00005B90, dwParam2=00036DBB
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=007F5C90, dwParam2=00036FAA
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=00005C90, dwParam2=0003705E
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=007F5D90, dwParam2=00037289
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=00005D90, dwParam2=0003735B
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=007F5E90, dwParam2=000376B1
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=00005E90, dwParam2=00037783
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=007F5F90, dwParam2=00037A17
        wMsg=MIM_DATA, dwInstance=00000000, dwParam1=00005F90, dwParam2=00037B16
        */
        // who knows what 0x90 is
        if ((dwParam1 & 0xFF) == 0x90) {
            switch (dwParam1) {
            case 0x00005B90: // prev up
                m_playback_control->previous();
                break;
            case 0x00005C90: // next up
                m_playback_control->next();
                break;
            case 0x00005D90: // stop up
                m_playback_control->pause(true);
                break;
            case 0x00005E90: // play up
                m_playback_control->play_or_pause();
                break;
            case 0x00005F90: // rec up

                break;
            }
        }
        else if ((dwParam1 & 0xFF) == 0xE0) {
            DWORD vol = (dwParam1 >> 8) & 0xFF;
            float volPercentage = 0;
            if (vol != 0) {
                volPercentage = log((float)vol) / log((float)0x7F);
            }

            m_playback_control->set_volume((1.0f - volPercentage) * m_playback_control->volume_mute);
        }
    }
};

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    static_api_ptr_t<main_thread_callback_manager> callback_manager;

    switch (wMsg) {
    case MIM_OPEN:
        console::printf("wMsg=MIM_OPEN\n");
        break;
    case MIM_CLOSE:
        console::printf("wMsg=MIM_CLOSE\n");
        break;
    case MIM_DATA:
        console::printf("wMsg=MIM_DATA, dwInstance=%08x, dwParam1=%08x, dwParam2=%08x\n", dwInstance, dwParam1, dwParam2);
        {
            service_ptr_t<midi_control_callback> cb = new service_impl_t<midi_control_callback>(wMsg, dwInstance, dwParam1, dwParam2);
            callback_manager->add_callback(cb);
        }
        break;
    case MIM_LONGDATA:
        console::printf("wMsg=MIM_LONGDATA\n");
        break;
    case MIM_ERROR:
        console::printf("wMsg=MIM_ERROR\n");
        break;
    case MIM_LONGERROR:
        console::printf("wMsg=MIM_LONGERROR\n");
        break;
    case MIM_MOREDATA:
        console::printf("wMsg=MIM_MOREDATA\n");
        break;
    default:
        console::printf("wMsg = unknown\n");
        break;
    }
    return;
}

class mainmenu_commands_in : public mainmenu_commands {
private:
    void StopMidi() {
        midiInStop(hMidiDevice);
        midiInClose(hMidiDevice);
        hMidiDevice = NULL;
        selectedMidiInDevice = -1;
    }

    void StartMidi(t_uint32 midiPort) {
        UINT nMidiDeviceNum = midiInGetNumDevs();
        if (nMidiDeviceNum == 0) {
            popup_message::g_show("midiInGetNumDevs() == 0.", "MIDI Control");
            return;
        }

        MMRESULT rv = midiInOpen(&hMidiDevice, midiPort, (DWORD)(void*)MidiInProc, 0, CALLBACK_FUNCTION);
        if (rv != MMSYSERR_NOERROR) {
            popup_message::g_show("midiInOpen() failed.", "MIDI Control");
            return;
        }

        midiInStart(hMidiDevice);
        selectedMidiInDevice = midiPort;
    }

public:
    t_uint32 get_command_count() {
        return midiInGetNumDevs();
    }
    GUID get_command(t_uint32 p_index) {
        // {6BB0D85A-7528-454E-AD96-026DCC0DB4F0}
        static const GUID guid_no = { 0x6bb0d85a, 0x7528, 0x454e,{ 0xad, 0x96, 0x2, 0x6d, 0xcc, 0xd, 0xb4, 0xf0 } };

        return guid_no;
    }
    void get_name(t_uint32 p_index, pfc::string_base & p_out) {
        MIDIINCAPS caps;

        midiInGetDevCaps(p_index, &caps, sizeof(MIDIINCAPS));

#define BUF_SIZE 1000
        char buf[BUF_SIZE];
        pfc::stringcvt::convert_wide_to_utf8(buf, BUF_SIZE, caps.szPname, sizeof(caps.szPname));

        p_out = pfc::string8(buf);
    }
    bool get_display(t_uint32 p_index, pfc::string_base & p_text, t_uint32 & p_flags) {
        p_flags = 0;
        if (p_index == selectedMidiInDevice) {
            p_flags |= flag_checked;
        }
        get_name(p_index, p_text);
        return true;
    }
    bool get_description(t_uint32 p_index, pfc::string_base & p_out) {
        p_out = "Start/stop MIDI listening.";
        return true;
    }
    GUID get_parent() {
        return g_mainmenu_group_id;
    }
    void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) {
        if (hMidiDevice != NULL) {
            StopMidi();
        }
        else {
            StartMidi(p_index);
        }
    }
};

class mainmenu_commands_out : public mainmenu_commands {
private:
    void StopMidi() {
        selectedMidiOutDevice = -1;
        midiOutClose(hMidiOutDevice);
        hMidiOutDevice = NULL;
    }

    void StartMidi(t_uint32 midiPort) {
        UINT nMidiDeviceNum = midiOutGetNumDevs();
        if (nMidiDeviceNum == 0) {
            popup_message::g_show("midiOutGetNumDevs() == 0.", "MIDI Control");
            return;
        }

        MMRESULT rv = midiOutOpen(&hMidiOutDevice, midiPort, (DWORD)(void*)MidiInProc, 0, CALLBACK_FUNCTION);
        if (rv != MMSYSERR_NOERROR) {
            popup_message::g_show("midiOutOpen() failed.", "MIDI Control");
            return;
        }

        selectedMidiOutDevice = midiPort;
    }

public:
    t_uint32 get_command_count() {
        return midiOutGetNumDevs();
    }
    GUID get_command(t_uint32 p_index) {
        // {23F0ED4B-E3CE-47FF-A4C1-47D595029013}
        static const GUID guid_no = { 0x23f0ed4b, 0xe3ce, 0x47ff,{ 0xa4, 0xc1, 0x47, 0xd5, 0x95, 0x2, 0x90, 0x13 } };

        return guid_no;
    }
    void get_name(t_uint32 p_index, pfc::string_base & p_out) {
        MIDIOUTCAPS caps;

        midiOutGetDevCaps(p_index, &caps, sizeof(MIDIOUTCAPS));

#define BUF_SIZE 1000
        char buf[BUF_SIZE];
        pfc::stringcvt::convert_wide_to_utf8(buf, BUF_SIZE, caps.szPname, sizeof(caps.szPname));

        p_out = pfc::string8(buf);
    }
    bool get_display(t_uint32 p_index, pfc::string_base & p_text, t_uint32 & p_flags) {
        p_flags = 0;
        if (p_index == selectedMidiOutDevice) {
            p_flags |= flag_checked;
        }
        get_name(p_index, p_text);
        return true;
    }
    bool get_description(t_uint32 p_index, pfc::string_base & p_out) {
        p_out = "Start/stop MIDI output.";
        return true;
    }
    GUID get_parent() {
        return g_mainmenu_group_out_id;
    }
    void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) {
        if (hMidiOutDevice != NULL) {
            StopMidi();
        }
        else {
            StartMidi(p_index);
        }
    }
};

static mainmenu_commands_factory_t<mainmenu_commands_in> g_mainmenu_commands_sample_factory;
static mainmenu_commands_factory_t<mainmenu_commands_out> g_mainmenu_commands_sample_out_factory;
