#pragma once

#include <memory>

#include <wx/timer.h>
#include <wx/wx.h>

class SpekSpectrogram;

class SpekWindow : public wxFrame
{
public:
    SpekWindow(const wxString& path);
    ~SpekWindow() override;
    void open(const wxString& path);

private:
    void on_erase_background(wxEraseEvent& event);
    void on_paint(wxPaintEvent& event);
    void on_open(wxCommandEvent& event);
    void on_save(wxCommandEvent& event);
    void on_exit(wxCommandEvent& event);
    void on_preferences(wxCommandEvent& event);
    void on_show_toolbar(wxCommandEvent& event);
    void on_help(wxCommandEvent& event);
    void on_check_updates(wxCommandEvent& event);
    void on_update_timer(wxTimerEvent& event);
    void on_about(wxCommandEvent& event);
    void on_visit(wxCommandEvent& event);
    void on_close(wxCommandEvent& event);
    void show_update_bar();

    struct UpdateRequest;
    std::shared_ptr<UpdateRequest> update_request;
    wxTimer update_timer;
    wxToolBar *toolbar = nullptr;
    wxMenuItem *check_updates_item = nullptr;
    SpekSpectrogram *spectrogram;
    wxString path;
    wxString cur_dir;
    wxString description;

    DECLARE_EVENT_TABLE()
};
