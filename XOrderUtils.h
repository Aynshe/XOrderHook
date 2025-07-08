#pragma once

#include <Windows.h>

// Fonction pour détecter la langue du système / Function to detect system language
inline bool IsSystemLanguageFrench() {
    LANGID langId = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langId);
    return (primaryLang == LANG_FRENCH);
}

// Fonction pour obtenir un message localisé simple / Function to get a simple localized message
inline const wchar_t* GetLocalizedMessage(const wchar_t* fr_message, const wchar_t* en_message) {
    return IsSystemLanguageFrench() ? fr_message : en_message;
}
