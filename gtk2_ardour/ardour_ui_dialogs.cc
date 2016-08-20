/*
    Copyright (C) 2000 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/* This file contains any ARDOUR_UI methods that require knowledge of
   the various dialog boxes, and exists so that no compilation dependency
   exists between the main ARDOUR_UI modules and their respective classes.
   This is to cut down on the compile times.  It also helps with my sanity.
*/

#include <vector>

#include "pbd/convert.h"

#include "ardour/audioengine.h"
#include "ardour/automation_watch.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "control_protocol/control_protocol.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"

#include "actions.h"
#include "add_route_dialog.h"
#include "add_video_dialog.h"
#include "ardour_ui.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "lua_script_manager.h"
#include "luawindow.h"
#include "main_clock.h"
#include "meterbridge.h"
#include "meter_patterns.h"
#include "monitor_section.h"
#include "midi_tracer.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "processor_box.h"
#include "rc_option_editor.h"
#include "route_params_ui.h"
#include "shuttle_control.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#include "splash.h"
#include "sfdb_ui.h"
#include "theme_manager.h"
#include "time_info_box.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;

void
ARDOUR_UI::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		WM::Manager::instance().set_session (s);
		/* Session option editor cannot exist across change-of-session */
		session_option_editor.drop_window ();
		/* Ditto for AddVideoDialog */
		add_video_dialog.drop_window ();
		return;
	}

	const XMLNode* node = _session->extra_xml (X_("UI"));

	if (node) {
		const XMLNodeList& children = node->children();
		for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
			if ((*i)->name() == GUIObjectState::xml_node_name) {
				gui_object_state->load (**i);
				break;
			}
		}
	}

	WM::Manager::instance().set_session (s);

	AutomationWatch::instance().set_session (s);

	if (shuttle_box) {
		shuttle_box->set_session (s);
	}

	primary_clock->set_session (s);
	secondary_clock->set_session (s);
	big_clock->set_session (s);
	time_info_box->set_session (s);
	video_timeline->set_session (s);

	/* sensitize menu bar options that are now valid */

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, true);
	ActionManager::set_sensitive (ActionManager::write_sensitive_actions, _session->writable());

	if (_session->locations()->num_range_markers()) {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
	} else {
		ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
	}

	if (!_session->monitor_out()) {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("options"), X_("SoloViaBus"));
		if (act) {
			act->set_sensitive (false);
		}
	}

	/* allow wastebasket flush again */

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("FlushWastebasket"));
	if (act) {
		act->set_sensitive (true);
	}

	/* there are never any selections on startup */

	ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::line_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::point_selection_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::playlist_selection_sensitive_actions, false);

	rec_button.set_sensitive (true);

	solo_alert_button.set_active (_session->soloing());

	setup_session_options ();

	blink_connection = Timers::blink_connect (sigc::mem_fun(*this, &ARDOUR_UI::blink_handler));

	_session->SaveSessionRequested.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::save_session_at_its_request, this, _1), gui_context());
	_session->RecordStateChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::record_state_changed, this), gui_context());
	_session->StepEditStatusChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::step_edit_status_change, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::map_transport_state, this), gui_context());
	_session->DirtyChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_dirty_changed, this), gui_context());

	_session->Xrun.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::xrun_handler, this, _1), gui_context());
	_session->SoloActive.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::soloing_changed, this, _1), gui_context());
	_session->AuditionActive.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::auditioning_changed, this, _1), gui_context());
	_session->locations()->added.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::handle_locations_change, this, _1), gui_context());
	_session->locations()->removed.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::handle_locations_change, this, _1), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::session_parameter_changed, this, _1), gui_context ());

	/* Clocks are on by default after we are connected to a session, so show that here.
	*/

	connect_dependents_to_session (s);

	/* listen to clock mode changes. don't do this earlier because otherwise as the clocks
	   restore their modes or are explicitly set, we will cause the "new" mode to be saved
	   back to the session XML ("Extra") state.
	 */

	AudioClock::ModeChanged.connect (sigc::mem_fun (*this, &ARDOUR_UI::store_clock_modes));

	Glib::signal_idle().connect (sigc::mem_fun (*this, &ARDOUR_UI::first_idle));

	start_clocking ();

	map_transport_state ();

	second_connection = Timers::second_connect (sigc::mem_fun(*this, &ARDOUR_UI::every_second));
	point_one_second_connection = Timers::rapid_connect (sigc::mem_fun(*this, &ARDOUR_UI::every_point_one_seconds));
	point_zero_something_second_connection = Timers::super_rapid_connect (sigc::mem_fun(*this, &ARDOUR_UI::every_point_zero_something_seconds));
	set_fps_timeout_connection();

	update_format ();

	if (meter_box.get_parent()) {
		transport_hbox.remove (meter_box);
		transport_hbox.remove (editor_meter_peak_display);
	}

	if (editor_meter) {
		meter_box.remove(*editor_meter);
		delete editor_meter;
		editor_meter = 0;
		editor_meter_peak_display.hide();
	}

	if (meter_box.get_parent()) {
		transport_hbox.remove (meter_box);
		transport_hbox.remove (editor_meter_peak_display);
	}

	if (_session &&
	    _session->master_out() &&
	    _session->master_out()->n_outputs().n(DataType::AUDIO) > 0) {

		if (!ARDOUR::Profile->get_trx()) {
			editor_meter = new LevelMeterHBox(_session);
			editor_meter->set_meter (_session->master_out()->shared_peak_meter().get());
			editor_meter->clear_meters();
			editor_meter->set_type (_session->master_out()->meter_type());
			editor_meter->setup_meters (30, 12, 6);
			editor_meter->show();
			meter_box.pack_start(*editor_meter);
		}

		ArdourMeter::ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &ARDOUR_UI::reset_peak_display));
		ArdourMeter::ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &ARDOUR_UI::reset_route_peak_display));
		ArdourMeter::ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &ARDOUR_UI::reset_group_peak_display));

		editor_meter_peak_display.set_name ("meterbridge peakindicator");
		editor_meter_peak_display.unset_flags (Gtk::CAN_FOCUS);
		editor_meter_peak_display.set_size_request (std::max(9.f, rintf(8.f * UIConfiguration::instance().get_ui_scale())), -1);
		editor_meter_peak_display.set_corner_radius (3.0);

		editor_meter_max_peak = -INFINITY;
		editor_meter_peak_display.signal_button_release_event().connect (sigc::mem_fun(*this, &ARDOUR_UI::editor_meter_peak_button_release), false);

		if (UIConfiguration::instance().get_show_editor_meter() && !ARDOUR::Profile->get_trx()) {
			transport_hbox.pack_start (meter_box, false, false);
			transport_hbox.pack_start (editor_meter_peak_display, false, false);
			meter_box.show();
			editor_meter_peak_display.show();
		}
	}

	update_title ();
}

int
ARDOUR_UI::unload_session (bool hide_stuff)
{
	if (_session) {
		ARDOUR_UI::instance()->video_timeline->sync_session_state();
	}

	if (_session && _session->dirty()) {
		std::vector<std::string> actions;
		actions.push_back (_("Don't close"));
		actions.push_back (_("Just close"));
		actions.push_back (_("Save and close"));
		switch (ask_about_saving_session (actions)) {
		case -1:
			// cancel
			return 1;

		case 1:
			_session->save_state ("");
			break;
		}
	}

	{
		// tear down session specific CPI (owned by rc_config_editor which can remain)
		ControlProtocolManager& m = ControlProtocolManager::instance ();
		for (std::list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {
			if (*i && (*i)->protocol && (*i)->protocol->has_editor ()) {
				(*i)->protocol->tear_down_gui ();
			}
		}
	}

	if (hide_stuff) {
		editor->hide ();
		mixer->hide ();
		meterbridge->hide ();
		audio_port_matrix->hide();
		midi_port_matrix->hide();
		route_params->hide();
	}

	second_connection.disconnect ();
	point_one_second_connection.disconnect ();
	point_zero_something_second_connection.disconnect();
	fps_connection.disconnect();

	if (editor_meter) {
		meter_box.remove(*editor_meter);
		delete editor_meter;
		editor_meter = 0;
		editor_meter_peak_display.hide();
	}

	ActionManager::set_sensitive (ActionManager::session_sensitive_actions, false);

	rec_button.set_sensitive (false);

	WM::Manager::instance().set_session ((ARDOUR::Session*) 0);

	if (ARDOUR_UI::instance()->video_timeline) {
		ARDOUR_UI::instance()->video_timeline->close_session();
	}

	stop_clocking ();

	/* drop everything attached to the blink signal */

	blink_connection.disconnect ();

	delete _session;
	_session = 0;

	session_loaded = false;

	update_buffer_load ();
	update_title ();

	return 0;
}

void
ARDOUR_UI::toggle_editor_and_mixer ()
{
	if (editor->tabbed() && mixer->tabbed()) {
		/* both in the same window */
		if (_tabs.get_current_page() == _tabs.page_num (editor->contents())) {
			_tabs.set_current_page (_tabs.page_num (mixer->contents()));
		} else if (_tabs.get_current_page() == _tabs.page_num (mixer->contents())) {
			_tabs.set_current_page (_tabs.page_num (editor->contents()));
		} else {
			/* go to mixer */
			_tabs.set_current_page (_tabs.page_num (mixer->contents()));
		}
		return;
	}


	if (editor->tabbed() && !mixer->tabbed()) {
		/* editor is tabbed, mixer is not */

		Gtk::Window* mwin = mixer->current_toplevel ();

		if (!mwin) {
			/* mixer's own window doesn't exist */
			mixer->make_visible ();
		} else if (!mwin->is_mapped ()) {
			/* mixer's own window exists but isn't mapped */
			mixer->make_visible ();
		} else {
			/* mixer window is mapped, editor is visible as tab */
			Gtk::Widget* f = mwin->get_focus();
			if (f && f->has_focus()) {
				/* mixer has focus, switch to editor */
				editor->make_visible ();
			} else {
				mixer->make_visible ();
			}
		}
		return;
	}

	if (!editor->tabbed() && mixer->tabbed()) {
		/* mixer is tabbed, editor is not */

		Gtk::Window* ewin = editor->current_toplevel ();

		if (!ewin) {
			/* mixer's own window doesn't exist */
			editor->make_visible ();
		} else if (!ewin->is_mapped ()) {
			/* editor's own window exists but isn't mapped */
			editor->make_visible ();
		} else {
			/* editor window is mapped, mixer is visible as tab */
			Gtk::Widget* f = ewin->get_focus();
			if (f && f->has_focus()) {
				/* editor has focus, switch to mixer */
				mixer->make_visible ();
			} else {
				editor->make_visible ();
			}
		}
		return;
	}
}

void
ARDOUR_UI::step_up_through_tabs ()
{
	std::vector<Tabbable*> candidates;

	/* this list must match the order of visibility buttons */

	if (!editor->window_visible()) {
		candidates.push_back (editor);
	}

	if (!mixer->window_visible()) {
		candidates.push_back (mixer);
	}

	if (!rc_option_editor->window_visible()) {
		candidates.push_back (rc_option_editor);
	}

	if (candidates.size() < 2) {
		/* nothing to be done with zero or one visible in tabs */
		return;
	}

	std::vector<Tabbable*>::iterator prev = candidates.end();
	std::vector<Tabbable*>::iterator i;
	Gtk::Widget* w = _tabs.get_nth_page (_tabs.get_current_page ());

	for (i = candidates.begin(); i != candidates.end(); ++i) {
		if (w == &(*i)->contents()) {
			if (prev != candidates.end()) {
				_tabs.set_current_page (_tabs.page_num ((*prev)->contents()));
			} else {
				_tabs.set_current_page (_tabs.page_num (candidates.back()->contents()));
			}
			return;
		}
		prev = i;
	}
}

void
ARDOUR_UI::step_down_through_tabs ()
{
	std::vector<Tabbable*> candidates;

	/* this list must match the order of visibility buttons */

	if (!editor->window_visible()) {
		candidates.push_back (editor);
	}

	if (!mixer->window_visible()) {
		candidates.push_back (mixer);
	}

	if (!rc_option_editor->window_visible()) {
		candidates.push_back (rc_option_editor);
	}

	if (candidates.size() < 2) {
		/* nothing to be done with zero or one visible in tabs */
		return;
	}

	std::vector<Tabbable*>::reverse_iterator next = candidates.rend();
	std::vector<Tabbable*>::reverse_iterator i;
	Gtk::Widget* w = _tabs.get_nth_page (_tabs.get_current_page ());

	for (i = candidates.rbegin(); i != candidates.rend(); ++i) {
		if (w == &(*i)->contents()) {
			if (next != candidates.rend()) {
				_tabs.set_current_page (_tabs.page_num ((*next)->contents()));
			} else {
				_tabs.set_current_page (_tabs.page_num (candidates.front()->contents()));
			}
			break;
		}
		next = i;
	}
}

void
ARDOUR_UI::key_change_tabbable_visibility (Tabbable* t)
{
	if (!t) {
		return;
	}

	if (t->tabbed()) {
		_tabs.set_current_page (_tabs.page_num (t->contents()));
	} else if (!t->fully_visible()) {
		t->make_visible ();
	} else {
		_main_window.present ();
	}
}

void
ARDOUR_UI::button_change_tabbable_visibility (Tabbable* t)
{
	/* For many/most users, clicking a button in the main window will make it
	   the main/front/key window, which will change any stacking relationship they
	   were trying to modify by clicking on the button in the first
	   place. This button-aware method knows that click on
	   a button designed to show/hide a Tabbable that has its own window
	   will have made that window be obscured (as the main window comes to
	   the front). We therefore *hide* the Tabbable's window if it is even
	   partially visible, believing that this is likely because the
	   Tabbable window used to be front, the user clicked to change that,
	   and before we even get here, the main window has become front.
	*/

	if (!t) {
		return;
	}

	if (t->tabbed()) {
		_tabs.set_current_page (_tabs.page_num (t->contents()));
	} else if (t->visible()) {
		t->hide();
	} else {
		t->make_visible ();
	}
}

void
ARDOUR_UI::show_tabbable (Tabbable* t)
{
	if (!t) {
		return;
	}

	t->make_visible ();
}

void
ARDOUR_UI::hide_tabbable (Tabbable* t)
{
	if (!t) {
		return;
	}
	t->make_invisible ();
}

void
ARDOUR_UI::attach_tabbable (Tabbable* t)
{
	if (!t) {
		return;
	}

	t->attach ();
}

void
ARDOUR_UI::detach_tabbable (Tabbable* t)
{
	if (!t) {
		return;
	}
	t->detach ();
}

void
ARDOUR_UI::tabs_page_added (Widget*,guint)
{
	if (_tabs.get_n_pages() > 1) {

		std::vector<TargetEntry> drag_target_entries;
		drag_target_entries.push_back (TargetEntry ("tabbable"));

		editor_visibility_button.drag_source_set (drag_target_entries);
		mixer_visibility_button.drag_source_set (drag_target_entries);
		prefs_visibility_button.drag_source_set (drag_target_entries);

		editor_visibility_button.drag_source_set_icon (Gtkmm2ext::pixbuf_from_string (editor->name(),
		                                                                              Pango::FontDescription ("Sans 24"),
		                                                                              0, 0,
		                                                                              Gdk::Color ("red")));
		mixer_visibility_button.drag_source_set_icon (Gtkmm2ext::pixbuf_from_string (mixer->name(),
		                                                                             Pango::FontDescription ("Sans 24"),
		                                                                             0, 0,
		                                                                             Gdk::Color ("red")));
		prefs_visibility_button.drag_source_set_icon (Gtkmm2ext::pixbuf_from_string (rc_option_editor->name(),
		                                                                             Pango::FontDescription ("Sans 24"),
		                                                                             0, 0,
		                                                                             Gdk::Color ("red")));
	}
}

void
ARDOUR_UI::tabs_page_removed (Widget*, guint)
{
	if (_tabs.get_n_pages() < 2) {
		editor_visibility_button.drag_source_unset ();
		mixer_visibility_button.drag_source_unset ();
		prefs_visibility_button.drag_source_unset ();
	}
}

void
ARDOUR_UI::tabs_switch (GtkNotebookPage*, guint page)
{
	if (editor && (page == (guint) _tabs.page_num (editor->contents()))) {
		editor_visibility_button.set_active_state (Gtkmm2ext::ImplicitActive);

		if (mixer && (mixer->tabbed() || mixer->tabbed_by_default())) {
			mixer_visibility_button.set_active_state (Gtkmm2ext::Off);
		}

		if (rc_option_editor && (rc_option_editor->tabbed() || rc_option_editor->tabbed_by_default())) {
			prefs_visibility_button.set_active_state (Gtkmm2ext::Off);
		}
	} else if (mixer && (page == (guint) _tabs.page_num (mixer->contents()))) {

		if (editor && (editor->tabbed() || editor->tabbed_by_default())) {
			editor_visibility_button.set_active_state (Gtkmm2ext::Off);
		}

		mixer_visibility_button.set_active_state (Gtkmm2ext::ImplicitActive);

		if (rc_option_editor && (rc_option_editor->tabbed() || rc_option_editor->tabbed_by_default())) {
			prefs_visibility_button.set_active_state (Gtkmm2ext::Off);
		}

	} else if (page == (guint) _tabs.page_num (rc_option_editor->contents())) {

		if (editor && (editor->tabbed() || editor->tabbed_by_default())) {
			editor_visibility_button.set_active_state (Gtkmm2ext::Off);
		}

		if (mixer && (mixer->tabbed() || mixer->tabbed_by_default())) {
			mixer_visibility_button.set_active_state (Gtkmm2ext::Off);
		}

		prefs_visibility_button.set_active_state (Gtkmm2ext::ImplicitActive);
	}

}

void
ARDOUR_UI::tabbable_state_change (Tabbable& t)
{
	std::vector<std::string> insensitive_action_names;
	std::vector<std::string> sensitive_action_names;
	std::vector<std::string> active_action_names;
	std::vector<std::string> inactive_action_names;
	Glib::RefPtr<Action> action;
	std::string downcased_name = downcase (t.name());
	enum ViewState {
		Tabbed,
		Windowed,
		Hidden
	};
	ViewState vs;

	if (t.tabbed()) {

		insensitive_action_names.push_back (string_compose ("attach-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("show-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("detach-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("hide-%1", downcased_name));

		vs = Tabbed;

	} else if (t.tabbed_by_default ()) {

		insensitive_action_names.push_back (string_compose ("attach-%1", downcased_name));
		insensitive_action_names.push_back (string_compose ("hide-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("show-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("detach-%1", downcased_name));

		vs = Hidden;

	} else if (t.window_visible()) {

		insensitive_action_names.push_back (string_compose ("detach-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("show-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("attach-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("hide-%1", downcased_name));

		active_action_names.push_back (string_compose ("show-%1", downcased_name));
		inactive_action_names.push_back (string_compose ("hide-%1", downcased_name));

		vs = Windowed;

	} else {

		/* not currently visible. allow user to retab it or just make
		 * it visible.
		 */

		insensitive_action_names.push_back (string_compose ("detach-%1", downcased_name));
		insensitive_action_names.push_back (string_compose ("hide-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("show-%1", downcased_name));
		sensitive_action_names.push_back (string_compose ("attach-%1", downcased_name));

		active_action_names.push_back (string_compose ("hide-%1", downcased_name));
		inactive_action_names.push_back (string_compose ("show-%1", downcased_name));

		vs = Hidden;
	}

	for (std::vector<std::string>::iterator s = insensitive_action_names.begin(); s != insensitive_action_names.end(); ++s) {
		action = ActionManager::get_action (X_("Common"), (*s).c_str());
		if (action) {
			action->set_sensitive (false);
		}
	}

	for (std::vector<std::string>::iterator s = sensitive_action_names.begin(); s != sensitive_action_names.end(); ++s) {
		action = ActionManager::get_action (X_("Common"), (*s).c_str());
		if (action) {
			action->set_sensitive (true);
		}
	}

	ArdourButton* vis_button = 0;
	std::vector<ArdourButton*> other_vis_buttons;

	if (&t == editor) {
		vis_button = &editor_visibility_button;
		other_vis_buttons.push_back (&mixer_visibility_button);
		other_vis_buttons.push_back (&prefs_visibility_button);
	} else if (&t == mixer) {
		vis_button = &mixer_visibility_button;
		other_vis_buttons.push_back (&editor_visibility_button);
		other_vis_buttons.push_back (&prefs_visibility_button);
	} else if (&t == rc_option_editor) {
		vis_button = &prefs_visibility_button;
		other_vis_buttons.push_back (&editor_visibility_button);
		other_vis_buttons.push_back (&mixer_visibility_button);
	}

	if (!vis_button) {
		return;
	}

	switch (vs) {
	case Tabbed:
		vis_button->set_active_state (Gtkmm2ext::ImplicitActive);
		break;
	case Windowed:
		vis_button->set_active_state (Gtkmm2ext::ExplicitActive);
		break;
	case Hidden:
		vis_button->set_active_state (Gtkmm2ext::Off);
		break;
	}

	for (std::vector<ArdourButton*>::iterator b = other_vis_buttons.begin(); b != other_vis_buttons.end(); ++b) {
		(*b)->set_active_state (Gtkmm2ext::Off);
	}
}

void
ARDOUR_UI::toggle_meterbridge ()
{
	assert (editor && mixer && meterbridge);

	bool show = false;
	bool obscuring = false;

	if (meterbridge->not_visible ()) {
		show = true;
	} else if ((editor->window_visible() && ARDOUR_UI_UTILS::windows_overlap (editor->own_window(), meterbridge)) ||
	           (mixer->window_visible () && ARDOUR_UI_UTILS::windows_overlap (mixer->own_window(), meterbridge))) {
		obscuring = true;
	}

	if (obscuring && ((editor->own_window() && editor->own_window()->property_has_toplevel_focus()) ||
	                  (mixer->own_window() && mixer->own_window()->property_has_toplevel_focus()))) {
		show = true;
	}

	if (show) {
		meterbridge->show_window ();
		meterbridge->present ();
		meterbridge->raise ();
	} else {
		meterbridge->hide_window (NULL);
	}
}

void
ARDOUR_UI::toggle_luawindow ()
{
	assert (editor && luawindow);

	bool show = false;

	if (luawindow->not_visible ()) {
		show = true;
	}
	// TODO check overlap

	if (show) {
		luawindow->show_window ();
		luawindow->present ();
		luawindow->raise ();
	} else {
		luawindow->hide_window (NULL);
	}
}


void
ARDOUR_UI::new_midi_tracer_window ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Common"), X_("NewMIDITracer"));
	if (!act) {
		return;
	}

	std::list<MidiTracer*>::iterator i = _midi_tracer_windows.begin ();
	while (i != _midi_tracer_windows.end() && (*i)->get_visible() == true) {
		++i;
	}

	if (i == _midi_tracer_windows.end()) {
		/* all our MIDITracer windows are visible; make a new one */
		MidiTracer* t = new MidiTracer ();
		t->show_all ();
		_midi_tracer_windows.push_back (t);
	} else {
		/* re-use the hidden one */
		(*i)->show_all ();
	}
}

KeyEditor*
ARDOUR_UI::create_key_editor ()
{
	KeyEditor* kedit = new KeyEditor;

	for (std::list<Bindings*>::iterator b = Bindings::bindings.begin(); b != Bindings::bindings.end(); ++b) {
		kedit->add_tab ((*b)->name(), **b);
	}

	return kedit;
}

BundleManager*
ARDOUR_UI::create_bundle_manager ()
{
	return new BundleManager (_session);
}

AddVideoDialog*
ARDOUR_UI::create_add_video_dialog ()
{
	return new AddVideoDialog (_session);
}

SessionOptionEditor*
ARDOUR_UI::create_session_option_editor ()
{
	return new SessionOptionEditor (_session);
}

BigClockWindow*
ARDOUR_UI::create_big_clock_window ()
{
	return new BigClockWindow (*big_clock);
}

void
ARDOUR_UI::handle_locations_change (Location *)
{
	if (_session) {
		if (_session->locations()->num_range_markers()) {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, true);
		} else {
			ActionManager::set_sensitive (ActionManager::range_sensitive_actions, false);
		}
	}
}

bool
ARDOUR_UI::tabbed_window_state_event_handler (GdkEventWindowState* ev, void* object)
{
	if (object == editor) {

		if ((ev->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) &&
		    (ev->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
			if (big_clock_window) {
				big_clock_window->set_transient_for (*editor->own_window());
			}
		}

	} else if (object == mixer) {

		if ((ev->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) &&
		    (ev->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
			if (big_clock_window) {
				big_clock_window->set_transient_for (*mixer->own_window());
			}
		}
	}

	return false;
}

bool
ARDOUR_UI::editor_meter_peak_button_release (GdkEventButton* ev)
{
	if (ev->button == 1 && Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, Gtkmm2ext::Keyboard::PrimaryModifier|Gtkmm2ext::Keyboard::TertiaryModifier)) {
		ArdourMeter::ResetAllPeakDisplays ();
	} else if (ev->button == 1 && Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, Gtkmm2ext::Keyboard::PrimaryModifier)) {
		if (_session->master_out()) {
			ArdourMeter::ResetGroupPeakDisplays (_session->master_out()->route_group());
		}
	} else if (_session->master_out()) {
		ArdourMeter::ResetRoutePeakDisplays (_session->master_out().get());
	}
	return false;
}

void
ARDOUR_UI::toggle_mixer_space()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Common", "ToggleMaximalMixer");

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		if (tact->get_active()) {
			mixer->maximise_mixer_space ();
		} else {
			mixer->restore_mixer_space ();
		}
	}
}

void
ARDOUR_UI::toggle_mixer_list()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Common", "ToggleMixerList");

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
			mixer->show_mixer_list (tact->get_active());
	}
}

void
ARDOUR_UI::toggle_monitor_section_visibility ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action ("Common", "ToggleMonitorSection");

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
			mixer->show_monitor_section (tact->get_active());
	}
}