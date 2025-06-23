#pragma once

// This file contains global variables shared across the XOrderHook project.
// Ce fichier contient les variables globales partagées dans le projet XOrderHook.

// Enables detailed debug messages in the log and via OutputDebugString.
// Active les messages de débogage détaillés dans le log et via OutputDebugString.
extern bool g_VerboseLogging;

// Handle to the DLL module, used to get file paths.
// Handle vers le module DLL, utilisé pour obtenir les chemins de fichiers.
extern HMODULE g_hModule;
