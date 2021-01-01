#include "stdafx.h"

// Identifier of our context menu group. Substitute with your own when reusing code.
// {A1112903-6578-4B5C-891C-C8B28AB271C1}
static const GUID guid_mygroup = { 0xa1112903, 0x6578, 0x4b5c,{ 0x89, 0x1c, 0xc8, 0xb2, 0x8a, 0xb2, 0x71, 0xc1 } };


// Switch to contextmenu_group_factory to embed your commands in the root menu but separated from other commands.

//static contextmenu_group_factory g_mygroup(guid_mygroup, contextmenu_groups::root, 0);
static contextmenu_group_popup_factory g_mygroup(guid_mygroup, contextmenu_groups::root, "Export metadata", 0);

static void RunExportCommand(metadb_handle_list_cref data);

// Simple context menu item class.
class myitem : public contextmenu_item_simple {
public:
	enum {
		cmd_export = 0,
		cmd_total
	};
	GUID get_parent() {return guid_mygroup;}
	unsigned get_num_items() {return cmd_total;}
	void get_item_name(unsigned p_index,pfc::string_base & p_out) {
		switch(p_index) {
			case cmd_export: p_out = "Export..."; break;
			default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
		}
	}
	void context_command(unsigned p_index,metadb_handle_list_cref p_data,const GUID& p_caller) {
		switch(p_index) {
			case cmd_export:
				RunExportCommand(p_data);
				break;
			default:
				uBugCheck();
		}
	}
	// Overriding this is not mandatory. We're overriding it just to demonstrate stuff that you can do such as context-sensitive menu item labels.
	bool context_get_display(unsigned p_index,metadb_handle_list_cref p_data,pfc::string_base & p_out,unsigned & p_displayflags,const GUID & p_caller) {
		switch(p_index) {
			case cmd_export:
				if (!__super::context_get_display(p_index, p_data, p_out, p_displayflags, p_caller)) return false;
				// Example context sensitive label: append the count of selected items to the label.
				p_out << " : " << p_data.get_count() << " item";
				if (p_data.get_count() != 1) p_out << "s";
				p_out << " selected";
				return true;
			default: uBugCheck();
		}
	}
	GUID get_item_guid(unsigned p_index) {
		// These GUIDs identify our context menu items. Substitute with your own GUIDs when reusing code.
		// {FF166BA9-06E1-4DDA-83BF-3D46C788CC7D}
		static const GUID guid_export = { 0xff166ba9, 0x6e1, 0x4dda,{ 0x83, 0xbf, 0x3d, 0x46, 0xc7, 0x88, 0xcc, 0x7d } };
		switch(p_index) {
			case cmd_export: return guid_export;
			default: uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
		}

	}
	bool get_item_description(unsigned p_index,pfc::string_base & p_out) {
		switch(p_index) {
			case cmd_export:
				p_out = "Export metadata to JSON.";
				return true;
			default:
				uBugCheck(); // should never happen unless somebody called us with invalid parameters - bail
		}
	}
};

static contextmenu_item_factory_t<myitem> g_myitem_factory;

//
// ... Exporting code ...
//

// cpprestsdk
#include <cpprest/json.h>
using namespace web::json;

#include <locale>
#include <codecvt>
#include <string>

static void RunExportCommand(metadb_handle_list_cref data) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

	pfc::string_formatter message;
	if (data.get_count() > 0) {
		for(t_size walk = 0; walk < data.get_count(); ++walk) {
			auto thisFile = value::object();
			thisFile[U("filename")] = value::string(converter.from_bytes(data[walk]->get_path()));
			thisFile[U("length_seconds")] = value::number(data[walk]->get_length());
			auto meta = value::object();

			metadb_info_container::ptr info = data[walk]->get_info_ref();
			for (t_size mwalk = 0; mwalk < info->info().meta_get_count(); ++mwalk) {
				std::wstring name = converter.from_bytes(info->info().meta_enum_name(mwalk));

				t_size vcount = info->info().meta_enum_value_count(mwalk);
				value vval = value::null();
				if (vcount == 1) {
					vval = value::string(converter.from_bytes(info->info().meta_enum_value(mwalk, 0)));
				} else {
					vval = value::array();
					for (t_size vwalk = 0; vwalk < vcount; ++vwalk) {
						vval[vwalk] = value::string(converter.from_bytes(info->info().meta_enum_value(mwalk, vwalk)));
					}
				}
				meta[name] = vval;
			}
			thisFile[U("metadata")] = meta;
			std::wstring serialized = thisFile.serialize();
			message << serialized.c_str();
			message << "\n";
		}
	}
	popup_message::g_show(message, "Metadata");
}
