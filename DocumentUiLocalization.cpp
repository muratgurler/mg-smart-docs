#include "DocumentUiLocalization.h"

#include <string.h>

#include "P4Localization.h"

namespace mg::p4 {
namespace {

const char* pick(const char* tr, const char* en, const char* nl, const char* de,
                 const char* fr, const char* es, const char* pl) {
    return p4SelectText(tr, en, nl, de, fr, es, pl);
}

struct MessageTranslation {
    const char* english;
    const char* tr;
    const char* nl;
    const char* de;
    const char* fr;
    const char* es;
    const char* pl;
};

const MessageTranslation kExact[] = {
    {"Prepared profile is missing", "Hazır profil eksik", "Voorbereid profiel ontbreekt", "Vorbereitetes Profil fehlt", "Profil préparé manquant", "Falta el perfil preparado", "Brak gotowego profilu"},
    {"Exactly one prepared profile is required", "Tam olarak bir hazır profil gerekli", "Precies één voorbereid profiel vereist", "Genau ein vorbereitetes Profil erforderlich", "Un seul profil préparé est requis", "Se requiere exactamente un perfil preparado", "Wymagany jest dokładnie jeden gotowy profil"},
    {"Profile metadata is unavailable", "Profil üstverisi kullanılamıyor", "Profielmetadata niet beschikbaar", "Profilmetadaten nicht verfügbar", "Métadonnées du profil indisponibles", "Metadatos del perfil no disponibles", "Metadane profilu są niedostępne"},
    {"Only prepared MG profile JSON is accepted", "Yalnız hazırlanmış MG profil JSON kabul edilir", "Alleen voorbereid MG-profiel-JSON wordt geaccepteerd", "Nur vorbereitetes MG-Profil-JSON wird akzeptiert", "Seul le JSON de profil MG préparé est accepté", "Solo se acepta JSON de perfil MG preparado", "Akceptowany jest tylko gotowy JSON profilu MG"},
    {"Profile file is missing", "Profil dosyası eksik", "Profielbestand ontbreekt", "Profildatei fehlt", "Fichier profil manquant", "Falta el archivo de perfil", "Brak pliku profilu"},
    {"Profile JSON cannot be opened", "Profil JSON açılamıyor", "Profiel-JSON kan niet worden geopend", "Profil-JSON kann nicht geöffnet werden", "Impossible d'ouvrir le JSON du profil", "No se puede abrir el JSON del perfil", "Nie można otworzyć JSON profilu"},
    {"Prepared profile is not a JSON object", "Hazır profil bir JSON nesnesi değil", "Voorbereid profiel is geen JSON-object", "Vorbereitetes Profil ist kein JSON-Objekt", "Le profil préparé n'est pas un objet JSON", "El perfil preparado no es un objeto JSON", "Gotowy profil nie jest obiektem JSON"},
    {"Profile size mismatch", "Profil boyutu uyuşmuyor", "Profielgrootte komt niet overeen", "Profilgröße stimmt nicht überein", "Taille du profil incorrecte", "El tamaño del perfil no coincide", "Niezgodny rozmiar profilu"},
    {"Profile CRC32 mismatch", "Profil CRC32 uyuşmuyor", "CRC32 van profiel komt niet overeen", "Profil-CRC32 stimmt nicht überein", "CRC32 du profil incorrect", "El CRC32 del perfil no coincide", "Niezgodny CRC32 profilu"},
    {"Prepared mobile profile verified", "Hazır mobil profil doğrulandı", "Voorbereid mobiel profiel geverifieerd", "Vorbereitetes Mobilprofil geprüft", "Profil mobile préparé vérifié", "Perfil móvil preparado verificado", "Gotowy profil mobilny zweryfikowany"},
    {"Profile storage unavailable", "Profil depolama alanı kullanılamıyor", "Profielopslag niet beschikbaar", "Profilspeicher nicht verfügbar", "Stockage du profil indisponible", "Almacenamiento de perfil no disponible", "Pamięć profilu jest niedostępna"},
    {"Only one prepared profile is accepted per session", "Her oturumda yalnız bir hazır profil kabul edilir", "Per sessie wordt slechts één voorbereid profiel geaccepteerd", "Pro Sitzung wird nur ein vorbereitetes Profil akzeptiert", "Un seul profil préparé est accepté par session", "Solo se acepta un perfil preparado por sesión", "Na sesję akceptowany jest tylko jeden gotowy profil"},
    {"Profile exceeds 256 KB limit", "Profil 256 KB sınırını aşıyor", "Profiel overschrijdt 256 KB", "Profil überschreitet 256 KB", "Le profil dépasse 256 Ko", "El perfil supera 256 KB", "Profil przekracza 256 KB"},
    {"Flash write failed", "Flash yazma başarısız", "Schrijven naar flash mislukt", "Flash-Schreiben fehlgeschlagen", "Échec d'écriture flash", "Falló la escritura flash", "Błąd zapisu flash"},
    {"Upload aborted", "Aktarım iptal edildi", "Upload afgebroken", "Upload abgebrochen", "Envoi annulé", "Carga cancelada", "Przesyłanie przerwane"},
    {"Invalid or expired document session", "Geçersiz veya süresi dolmuş aktarım oturumu", "Ongeldige of verlopen importsessie", "Ungültige oder abgelaufene Importsitzung", "Session d'import invalide ou expirée", "Sesión de importación no válida o caducada", "Nieprawidłowa lub wygasła sesja importu"},
};

String translatePrefix(const String& input, const char* prefix,
                       const char* tr, const char* nl, const char* de,
                       const char* fr, const char* es, const char* pl) {
    if (!input.startsWith(prefix)) return String();
    String out = pick(tr, prefix, nl, de, fr, es, pl);
    out += input.substring(strlen(prefix));
    return out;
}

}  // namespace

String localizeDocumentUiMessage(const String& message) {
    if (message.isEmpty() || currentP4Language() == P4Language::English) return message;
    for (const auto& item : kExact) {
        if (message == item.english) {
            return String(pick(item.tr, item.english, item.nl, item.de, item.fr, item.es, item.pl));
        }
    }
    String prefixed = translatePrefix(message, "Profile JSON parse failed: ",
        "Profil JSON ayrıştırma başarısız: ", "Profiel-JSON parseren mislukt: ",
        "Profil-JSON konnte nicht gelesen werden: ", "Échec de lecture du JSON profil : ",
        "Falló el análisis del JSON de perfil: ", "Błąd parsowania JSON profilu: ");
    if (!prefixed.isEmpty()) return prefixed;
    prefixed = translatePrefix(message, "Electrical profile validation failed: ",
        "Elektriksel profil doğrulama başarısız: ", "Elektrische profielvalidatie mislukt: ",
        "Elektrische Profilprüfung fehlgeschlagen: ", "Échec de validation électrique du profil : ",
        "Falló la validación eléctrica del perfil: ", "Walidacja elektryczna profilu nie powiodła się: ");
    if (!prefixed.isEmpty()) return prefixed;
    return message;
}

const char* localizeDocumentUiMessage(const char* message) {
    if (message == nullptr || currentP4Language() == P4Language::English) return message != nullptr ? message : "";
    for (const auto& item : kExact) {
        if (strcmp(message, item.english) == 0) {
            return pick(item.tr, item.english, item.nl, item.de, item.fr, item.es, item.pl);
        }
    }
    return message;
}

}  // namespace mg::p4
