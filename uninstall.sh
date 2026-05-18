#!/usr/bin/env bash
set -euo pipefail

rm -f "$HOME/.local/lib/x86_64-linux-gnu/fcitx5/libuyghurkenlm.so"
rm -f "$HOME/.local/share/fcitx5/addon/uyghurkenlm.conf"
rm -f "$HOME/.local/share/fcitx5/inputmethod/uyghur-kenlm.conf"
rm -rf "$HOME/.local/share/fcitx5/themes/uyghur-kenlm"

echo "Removed Uyghur KenLM addon files."
echo "Data files are left in ~/.local/share/fcitx5/uyghur-kenlm so custom dictionaries are not deleted."
