#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="$HOME/.local/share/fcitx5/uyghur-kenlm"
LIB_DIR="$HOME/.local/lib/x86_64-linux-gnu/fcitx5"
ADDON_DIR="$HOME/.local/share/fcitx5/addon"
IM_DIR="$HOME/.local/share/fcitx5/inputmethod"
THEME_DIR="$HOME/.local/share/fcitx5/themes/uyghur-kenlm"
CLASSIC_CONF="$HOME/.config/fcitx5/conf/classicui.conf"

install -Dm755 "$ROOT/lib/libuyghurkenlm.so" "$LIB_DIR/libuyghurkenlm.so"
install -Dm755 "$ROOT/bin/uykenlm-static" "$DATA_DIR/uykenlm-static"
install -Dm644 "$ROOT/data/char_trie_q8.bin" "$DATA_DIR/char_trie_q8.bin"
install -Dm644 "$ROOT/data/dict.txt" "$DATA_DIR/dict.txt"
install -Dm644 "$ROOT/fcitx5/inputmethod/uyghur-kenlm.conf" "$IM_DIR/uyghur-kenlm.conf"

mkdir -p "$ADDON_DIR"
sed "s#^Library=.*#Library=$LIB_DIR/libuyghurkenlm#" \
  "$ROOT/fcitx5/addon/uyghurkenlm.conf" > "$ADDON_DIR/uyghurkenlm.conf"

mkdir -p "$THEME_DIR"
cp -a "$ROOT/theme/uyghur-kenlm/." "$THEME_DIR/"

mkdir -p "$(dirname "$CLASSIC_CONF")"
cat > "$CLASSIC_CONF" <<'EOF'
[Behavior]
Vertical Candidate List=True

[Appearance]
Font="KacstNaskh 14"
MenuFont="KacstNaskh 13"
TrayFont="Sans 10"
Theme=uyghur-kenlm
DarkTheme=uyghur-kenlm
EOF

PROFILE="$HOME/.config/fcitx5/profile"
if [ -f "$PROFILE" ] && ! grep -q 'Name=uyghur-kenlm' "$PROFILE"; then
  cp "$PROFILE" "$PROFILE.bak-uyghur-kenlm"
  python3 "$ROOT/tools/add_to_profile.py" "$PROFILE"
fi

echo "Installed Uyghur KenLM."
echo "Restart Fcitx5 with: fcitx5 -r -d"
echo "Dictionary: $DATA_DIR/dict.txt"
