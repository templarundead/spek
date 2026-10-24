#include "spek-config.h"

#include <mutex>
#include <string>
#include <thread>

#include <wx/aboutdlg.h>
#include <wx/artprov.h>
#include <wx/dcbuffer.h>
#include <wx/dnd.h>
#include <wx/filename.h>
#include <wx/protocol/http.h>
#include <wx/sstream.h>

#include "spek-utils.h"

#include "spek-artwork.h"
#include "spek-platform.h"
#include "spek-preferences-dialog.h"
#include "spek-preferences.h"
#include "spek-spectrogram.h"

#include "spek-window.h"

namespace
{
constexpr int ID_SHOW_TOOLBAR = wxID_HIGHEST + 1;
constexpr int ID_CHECK_UPDATES = wxID_HIGHEST + 2;
}

struct SpekWindow::UpdateRequest
{
    std::mutex mutex;
    bool finished = false;
    // 1: a newer version exists, 0: up to date, -1: request failed.
    int status = -1;
};

BEGIN_EVENT_TABLE(SpekWindow, wxFrame)
    EVT_ERASE_BACKGROUND(SpekWindow::on_erase_background)
    EVT_PAINT(SpekWindow::on_paint)
    EVT_MENU(wxID_OPEN, SpekWindow::on_open)
    EVT_MENU(wxID_SAVE, SpekWindow::on_save)
    EVT_MENU(wxID_EXIT, SpekWindow::on_exit)
    EVT_MENU(wxID_PREFERENCES, SpekWindow::on_preferences)
    EVT_MENU(ID_SHOW_TOOLBAR, SpekWindow::on_show_toolbar)
    EVT_MENU(wxID_HELP, SpekWindow::on_help)
    EVT_MENU(ID_CHECK_UPDATES, SpekWindow::on_check_updates)
    EVT_MENU(wxID_ABOUT, SpekWindow::on_about)
END_EVENT_TABLE()

// Returns 1 for an available update, 0 when up to date and -1 on failure.
static int check_version();

class SpekDropTarget : public wxFileDropTarget
{
public:
    SpekDropTarget(SpekWindow *window) : wxFileDropTarget(), window(window) {}

protected:
    virtual bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames){
        if (filenames.GetCount() == 1) {
            window->open(filenames[0]);
            return true;
        }
        return false;
    }

private:
    SpekWindow *window;
};

SpekWindow::SpekWindow(const wxString& path) :
    wxFrame(NULL, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize),
    update_timer(this),
    path(path)
{
    // Paint the frame as one buffered surface. In particular, do not let the
    // native Windows background erase expose a white frame in dark mode.
    this->SetExtraStyle(this->GetExtraStyle() | wxWS_EX_PROCESS_UI_UPDATES);
    this->SetBackgroundStyle(wxBG_STYLE_PAINT);
    this->SetDoubleBuffered(true);

    this->description = _("Spek - Acoustic Spectrum Analyser");
    SetTitle(this->description);
    const wxSize initial_size = this->FromDIP(wxSize(1024, 700));
    SetMinSize(initial_size);
    SetSize(initial_size);
    this->Bind(wxEVT_TIMER, &SpekWindow::on_update_timer, this);

#ifndef OS_OSX
    SetIcons(wxArtProvider::GetIconBundle(ART_SPEK, wxART_FRAME_ICON));
#endif

    wxMenuBar *menu = new wxMenuBar();

    wxMenu *menu_file = new wxMenu();
    wxMenuItem *menu_file_open = new wxMenuItem(menu_file, wxID_OPEN);
    menu_file->Append(menu_file_open);
    wxMenuItem *menu_file_save = new wxMenuItem(menu_file, wxID_SAVE);
    menu_file->Append(menu_file_save);
    menu_file->AppendSeparator();
    menu_file->Append(wxID_EXIT);
    menu->Append(menu_file, _("&File"));

    wxMenu *menu_options = new wxMenu();
    wxMenuItem *menu_options_prefs = new wxMenuItem(menu_options, wxID_PREFERENCES);
    menu_options_prefs->SetItemLabel(menu_options_prefs->GetItemLabelText() + "\tCtrl-E");
    menu_options->Append(menu_options_prefs);
    menu_options->AppendSeparator();
    wxMenuItem *menu_options_show = menu_options->AppendCheckItem(
        ID_SHOW_TOOLBAR, _("Show Toolbar"));
    menu->Append(menu_options, _("Options"));

    wxMenu *menu_help = new wxMenu();
    wxMenuItem *menu_help_contents = new wxMenuItem(
        menu_help, wxID_HELP, wxString(_("&Help")) + "\tF1");
    menu_help->Append(menu_help_contents);
    menu_help->AppendSeparator();
    this->check_updates_item = new wxMenuItem(
        menu_help, ID_CHECK_UPDATES, _("Check for Updates..."));
    menu_help->Append(this->check_updates_item);
    wxMenuItem *menu_help_about = new wxMenuItem(menu_help, wxID_ABOUT);
    menu_help_about->SetItemLabel(menu_help_about->GetItemLabelText() + "\tShift-F1");
    menu_help->Append(menu_help_about);
    menu->Append(menu_help, _("&Help"));

    SetMenuBar(menu);

    this->toolbar = CreateToolBar();
    toolbar->SetExtraStyle(toolbar->GetExtraStyle() | wxWS_EX_PROCESS_UI_UPDATES);
    toolbar->SetBackgroundStyle(wxBG_STYLE_PAINT);
    toolbar->SetDoubleBuffered(true);
    // wxToolBar is native on Windows and has no application wxPaintEvent:
    // WS_EX_COMPOSITED makes its native paint atomic. Custom paint handlers
    // (the frame and spectrogram) use wxAutoBufferedPaintDC instead.
    toolbar->Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent& event) {
        // The native toolbar paints itself, including its background. Do not
        // let a separate WM_ERASEBKGND fill it with the default light colour.
        event.Skip(false);
    });
    toolbar->AddTool(
        wxID_OPEN,
        wxEmptyString,
        wxArtProvider::GetBitmap(ART_OPEN, wxART_TOOLBAR),
        menu_file_open->GetItemLabelText()
    );
    toolbar->AddTool(
        wxID_SAVE,
        wxEmptyString,
        wxArtProvider::GetBitmap(ART_SAVE, wxART_TOOLBAR),
        menu_file_save->GetItemLabelText()
    );
    toolbar->AddStretchableSpace();
    toolbar->AddTool(
        wxID_HELP,
        wxEmptyString,
        wxArtProvider::GetBitmap(ART_HELP, wxART_TOOLBAR),
        _("Help")
    );
    toolbar->Realize();
    const bool show_toolbar = SpekPreferences::get().get_show_toolbar();
    menu_options_show->Check(show_toolbar);
    toolbar->Show(show_toolbar);

    wxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    // wxInfoBar is too limited, construct a custom one.
    wxPanel *info_bar = new wxPanel(this);
    info_bar->Hide();
    info_bar->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
    info_bar->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
    wxSizer *info_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *label = new wxStaticText(
        info_bar, -1, _("A new version of Spek is available, click to download."));
    label->SetCursor(*new wxCursor(wxCURSOR_HAND));
    label->Connect(wxEVT_LEFT_DOWN, wxCommandEventHandler(SpekWindow::on_visit));
    // This second Connect() handles clicks on the border
    info_bar->Connect(wxEVT_LEFT_DOWN, wxCommandEventHandler(SpekWindow::on_visit));
    info_sizer->Add(label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
    wxBitmapButton *button = new wxBitmapButton(
        info_bar, -1, wxArtProvider::GetBitmap(ART_CLOSE, wxART_BUTTON),
        wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SpekWindow::on_close));
    info_sizer->Add(button, 0, wxALIGN_CENTER_VERTICAL);
    info_bar->SetSizer(info_sizer);
    sizer->Add(info_bar, 0, wxEXPAND);

    this->spectrogram = new SpekSpectrogram(this);
    sizer->Add(this->spectrogram, 1, wxEXPAND);

    this->cur_dir = wxGetHomeDir();

    SetDropTarget(new SpekDropTarget(this));
    SetSizer(sizer);
    this->Layout();

    if (!path.IsEmpty()) {
        open(path);
    }

    // Draw the window decorations in dark mode when the OS uses it.
    spek_platform_enable_dark_mode(this);
}

SpekWindow::~SpekWindow()
{
    this->update_timer.Stop();
    this->update_request.reset();
}

void SpekWindow::on_erase_background(wxEraseEvent& event)
{
    // The paint handler below draws the complete frame background. Skipping
    // the native erase avoids a light-coloured intermediate frame on Windows.
    event.Skip(false);
}

void SpekWindow::on_paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
    dc.Clear();
}

void SpekWindow::open(const wxString& path)
{
    if (path.IsEmpty()) {
        return;
    }

    // FileExists() can itself wait for a cloud-backed or network path. Let
    // Audio::open() perform the real check in its worker and report any error
    // instead of blocking the window before it can repaint.
    wxFileName file_name(path);
    this->path = path;
    wxString full_name = file_name.GetFullName();
    // TRANSLATORS: window title, %s is replaced with the file name
    wxString title = wxString::Format(_("Spek - %s"), full_name.c_str());
    SetTitle(title);

    this->spectrogram->open(path);

    // SpekSpectrogram includes the file name in its best size. Grow only
    // when necessary so the complete name is not replaced by an ellipsis.
    const wxSize best_size = this->GetBestSize();
    wxSize window_size = this->GetSize();
    if (best_size.GetWidth() > window_size.GetWidth()) {
        window_size.SetWidth(best_size.GetWidth());
        this->SetSize(window_size);
    }
    this->Layout();
}

void SpekWindow::on_open(wxCommandEvent&)
{
    static wxString filters = wxEmptyString;
    static int filter_index = 1;

    if (filters.IsEmpty()) {
        filters.Alloc(1024);
        filters += _("All files");
        filters += "|*.*|";
        filters += _("Audio files");
        filters += "|";
        bool first = true;
        for (const auto& extension : spek_audio_extensions()) {
            if (!first) {
                filters += ";";
            }
            filters += "*.";
            filters += wxString::FromUTF8(std::string(extension).c_str());
            first = false;
        }
        filters.Shrink();
    }

    wxFileDialog *dlg = new wxFileDialog(
        this,
        _("Open File"),
        this->cur_dir,
        wxEmptyString,
        filters,
        wxFD_OPEN
    );
    dlg->SetFilterIndex(filter_index);

    if (dlg->ShowModal() == wxID_OK) {
        this->cur_dir = dlg->GetDirectory();
        filter_index = dlg->GetFilterIndex();
        open(dlg->GetPath());
    }

    dlg->Destroy();
}

void SpekWindow::on_save(wxCommandEvent&)
{
    static wxString filters = wxEmptyString;

    if (filters.IsEmpty()) {
        filters = _("PNG images");
        filters += "|*.png";
    }

    wxFileDialog *dlg = new wxFileDialog(
        this,
        _("Save Spectrogram"),
        this->cur_dir,
        wxEmptyString,
        filters,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    // Suggested name is <file_name>.png
    wxString name = _("Untitled");
    if (!this->path.IsEmpty()) {
        wxFileName file_name(this->path);
        name = file_name.GetFullName();
    }
    name += ".png";
    dlg->SetFilename(name);

    if (dlg->ShowModal() == wxID_OK) {
        this->cur_dir = dlg->GetDirectory();
        this->spectrogram->save(dlg->GetPath());
    }

    dlg->Destroy();
}

void SpekWindow::on_exit(wxCommandEvent&)
{
    Close(true);
}

void SpekWindow::on_preferences(wxCommandEvent&)
{
    SpekPreferencesDialog dlg(this);
    dlg.ShowModal();
}

void SpekWindow::on_show_toolbar(wxCommandEvent& event)
{
    const bool show = event.IsChecked();
    this->toolbar->Show(show);
    this->Layout();
    SpekPreferences::get().set_show_toolbar(show);
}

void SpekWindow::on_help(wxCommandEvent&)
{
    wxLaunchDefaultBrowser(
        wxString::Format("http://help.spek.cc/man-%s.html", SPEK_VERSION)
    );
}

void SpekWindow::on_check_updates(wxCommandEvent&)
{
    if (this->update_request) {
        return;
    }

    auto request = std::make_shared<UpdateRequest>();
    this->update_request = request;
    this->check_updates_item->Enable(false);
    this->update_timer.Start(25);

    // The request owns no window pointer. If the frame closes while the
    // network is busy, its timer disappears and the worker simply releases
    // the shared request when it finishes.
    std::thread([request]() {
        const int status = check_version();
        std::lock_guard<std::mutex> lock(request->mutex);
        request->status = status;
        request->finished = true;
    }).detach();
}

void SpekWindow::on_update_timer(wxTimerEvent&)
{
    auto request = this->update_request;
    if (!request) {
        this->update_timer.Stop();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(request->mutex);
        if (!request->finished) {
            return;
        }
    }

    this->update_timer.Stop();
    this->update_request.reset();
    this->check_updates_item->Enable(true);

    if (request->status > 0) {
        this->show_update_bar();
    } else if (request->status == 0) {
        wxMessageBox(
            _("You are using the latest version of Spek."),
            _("Check for Updates"),
            wxOK | wxICON_INFORMATION,
            this
        );
    } else {
        wxMessageBox(
            _("Unable to check for updates. Please try again later."),
            _("Check for Updates"),
            wxOK | wxICON_ERROR,
            this
        );
    }
}

void SpekWindow::show_update_bar()
{
    this->GetSizer()->Show((size_t)0);
    this->Layout();
}

void SpekWindow::on_about(wxCommandEvent&)
{
    wxAboutDialogInfo info;
    info.AddDeveloper("Alexander Kojevnikov");
    info.AddDeveloper("Andreas Cadhalpun");
    info.AddDeveloper("Colin Watson");
    info.AddDeveloper("Daniel Hams");
    info.AddDeveloper("Elias Ojala");
    info.AddDeveloper("Fabian Deutsch");
    info.AddDeveloper("Guillaume Fourrier");
    info.AddDeveloper("Jakov Smolic");
    info.AddDeveloper("Jonathan Gonzalez V");
    info.AddDeveloper("Matteo Bini");
    info.AddDeveloper("Mike Wang");
    info.AddDeveloper("Simon Ruderich");
    info.AddDeveloper("Stefan Kost");
    info.AddDeveloper("Thibault North");
    info.AddDeveloper("Wyatt J. Brown");
    info.AddArtist("Olga Vasylevska");
    // TRANSLATORS: Add your name here
    wxString translator = _("translator-credits");
    if (translator != "translator-credits") {
        info.AddTranslator(translator);
    }
    info.SetName("Spek");
    info.SetVersion(SPEK_VERSION);
    info.SetCopyright(_("Copyright (c) 2010-2013 Alexander Kojevnikov and contributors"));
    info.SetDescription(this->description);
#ifdef OS_UNIX
    info.SetWebSite("https://www.spek.cc/", _("Spek Website"));
    info.SetIcon(wxArtProvider::GetIcon(ART_SPEK, wxART_OTHER, wxSize(128, 128)));
#endif
    wxAboutBox(info);
}

void SpekWindow::on_visit(wxCommandEvent&)
{
    wxLaunchDefaultBrowser("https://www.spek.cc");
}

void SpekWindow::on_close(wxCommandEvent& event)
{
    wxWindow *self = ((wxWindow *)event.GetEventObject())->GetGrandParent();
    self->GetSizer()->Hide((size_t)0);
    self->Layout();
}

static int check_version()
{
    wxString version;
    wxHTTP http;
    http.SetTimeout(10);
    if (http.Connect("help.spek.cc")) {
        wxInputStream *stream = http.GetInputStream("/version");
        if (stream) {
            wxStringOutputStream out(&version);
            stream->Read(out);
            version.Trim();
            delete stream;
        }
    }

    if (version.IsEmpty()) {
        return -1;
    }
    return spek_vercmp(version.mb_str(wxConvLibc).data(), SPEK_VERSION) == 1 ? 1 : 0;
}
