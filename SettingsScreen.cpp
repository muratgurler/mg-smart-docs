#include "SettingsScreen.h"
#include <Arduino.h>
#include "ConfigP4.h"
#include "P4Fonts.h"
#include "P4Localization.h"
namespace mg::p4 {
namespace {
void flat(lv_obj_t* o){ lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_style_pad_all(o,0,LV_PART_MAIN); }
lv_obj_t* text(lv_obj_t* p,const char* t,const lv_font_t* f,lv_color_t c){ auto* l=lv_label_create(p); lv_label_set_text(l,t); lv_obj_set_style_text_font(l,f,LV_PART_MAIN); lv_obj_set_style_text_color(l,c,LV_PART_MAIN); return l; }
}
void SettingsScreen::begin(){
 pendingAction_=SettingsAction::None;
 if(!screen_) screen_=lv_obj_create(nullptr); else lv_obj_clean(screen_);
 lv_scr_load(screen_); flat(screen_); lv_obj_set_style_bg_color(screen_,lv_color_hex(0x081119),LV_PART_MAIN); lv_obj_set_style_bg_opa(screen_,LV_OPA_COVER,LV_PART_MAIN);
 auto* h=lv_obj_create(screen_); lv_obj_set_pos(h,0,0); lv_obj_set_size(h,kDisplayWidth,82); flat(h); lv_obj_set_style_bg_color(h,lv_color_hex(0x142B3A),LV_PART_MAIN); lv_obj_set_style_bg_opa(h,LV_OPA_COVER,LV_PART_MAIN); lv_obj_set_style_border_width(h,0,LV_PART_MAIN);
 const auto& t=p4Texts(); auto* title=text(h,t.settingsTitle,p4Font20(),lv_color_hex(0xEAF6FF)); lv_obj_align(title,LV_ALIGN_TOP_MID,0,12); auto* sub=text(h,t.settingsSubtitle,p4Font14(),lv_color_hex(0x9CC6DC)); lv_obj_align(sub,LV_ALIGN_BOTTOM_MID,0,-10); soundButton_.create(h,970,18,42);
 addButton(screen_,40,125,t.touchDiagnostic,t.diagnosticInfo,lv_color_hex(0x167A9A),SettingsAction::TouchDiagnostic,true,0);
 addButton(screen_,366,125,t.wifi,"ESP32-C6 / ESP-Hosted",lv_color_hex(0x176B55),SettingsAction::Wifi,true,1);
 addButton(screen_,692,125,t.ethernet,"IP101 / RMII",lv_color_hex(0x72521A),SettingsAction::Ethernet,true,2);
 addButton(screen_,40,300,
           p4SelectText("WORKPLACE SERVER", "WORKPLACE SERVER", "WORKPLACE-SERVER", "WORKPLACE-SERVER", "SERVEUR WORKPLACE", "SERVIDOR WORKPLACE", "SERWER WORKPLACE"),
           p4SelectText("PR URL, kimlik doğrulama, CA ve cevap ayarları", "PR URL, authentication, CA and response settings", "PR-URL, authenticatie, CA en antwoordinstellingen", "PR-URL, Authentifizierung, CA und Antwort-Einstellungen", "URL PR, authentification, CA et paramètres de réponse", "URL PR, autenticación, CA y ajustes de respuesta", "URL PR, uwierzytelnianie, CA i ustawienia odpowiedzi"),
           lv_color_hex(0x325E7A),SettingsAction::WorkplaceServer,true,3);
 addButton(screen_,366,300,
           p4SelectText("DONANIM / PIN FREEZE", "HARDWARE / PIN FREEZE", "HARDWARE / PIN FREEZE", "HARDWARE / PIN FREEZE", "MATÉRIEL / PIN FREEZE", "HARDWARE / PIN FREEZE", "SPRZĘT / PIN FREEZE"),
           p4SelectText("GPIO, bus ve PCB doğrulama omurgası", "GPIO, bus and PCB validation backbone", "GPIO-, bus- en PCB-validatie", "GPIO-, Bus- und PCB-Validierung", "Validation GPIO, bus et PCB", "Validación de GPIO, bus y PCB", "Walidacja GPIO, magistral i PCB"),
           lv_color_hex(0x4E6170),SettingsAction::HardwareStatus,true,4);
 addButton(screen_,40,475,t.back,"",lv_color_hex(0x6A4B8A),SettingsAction::Back,true,5);
 Serial.println("[SETTINGS] Settings menu ready"); Serial.flush();
}
lv_obj_t* SettingsScreen::addButton(lv_obj_t* p,lv_coord_t x,lv_coord_t y,const char* title,const char* subtitle,lv_color_t c,SettingsAction a,bool en,uint8_t i){
 auto* b=lv_btn_create(p); lv_obj_set_pos(b,x,y); lv_obj_set_size(b,i==5?220:292,i==5?70:140); lv_obj_set_style_bg_color(b,c,LV_PART_MAIN); lv_obj_set_style_radius(b,12,LV_PART_MAIN); lv_obj_set_style_border_width(b,2,LV_PART_MAIN); lv_obj_set_style_border_color(b,en?lv_color_hex(0x75A7BD):lv_color_hex(0x52606A),LV_PART_MAIN); lv_obj_set_style_pad_all(b,0,LV_PART_MAIN);
 if(en){ bindings_[i]={this,a}; lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,&bindings_[i]); } else lv_obj_add_state(b,LV_STATE_DISABLED);
 auto* tl=text(b,title,p4Font20(),en?lv_color_hex(0xFFFFFF):lv_color_hex(0x9AA7AE)); lv_obj_set_width(tl,i==5?200:272); lv_obj_set_style_text_align(tl,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN); lv_obj_align(tl,LV_ALIGN_TOP_MID,0,i==5?20:30);
 if(i!=5){ auto* sl=text(b,subtitle,p4Font14(),en?lv_color_hex(0xD3E6EF):lv_color_hex(0x7F8C94)); lv_obj_set_width(sl,270); lv_obj_set_style_text_align(sl,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN); lv_obj_align(sl,LV_ALIGN_BOTTOM_MID,0,-22); }
 return b;
}
void SettingsScreen::callback(lv_event_t* e){ auto* b=static_cast<Binding*>(lv_event_get_user_data(e)); if(b&&b->owner)b->owner->request(b->action); }
void SettingsScreen::request(SettingsAction a){ pendingAction_=a; Serial.printf("[SETTINGS] action=%u\n",static_cast<unsigned>(a)); Serial.flush(); }
void SettingsScreen::activate(){ pendingAction_=SettingsAction::None; if(screen_&&lv_scr_act()!=screen_)lv_scr_load(screen_); }
SettingsAction SettingsScreen::consumeAction(){ auto a=pendingAction_; pendingAction_=SettingsAction::None; return a; }
} // namespace mg::p4
