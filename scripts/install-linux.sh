#!/usr/bin/env bash
# Установка «Гравитационных частиц» на Linux одной командой:
#   1) ставит необходимые пакеты (нужен пароль администратора — sudo);
#   2) собирает программу;
#   3) устанавливает её в ~/.local и добавляет ярлык в меню приложений.
# Удаление: scripts/install-linux.sh --uninstall
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build-release"

say() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
die() { printf '\n\033[1;31mОшибка: %s\033[0m\n' "$*" >&2; exit 1; }

if [[ "${1:-}" == "--uninstall" ]]; then
    say "Удаляю программу из $PREFIX"
    rm -f "$PREFIX/bin/gravity_particles" "$PREFIX/share/applications/gravity-particles.desktop"
    rm -rf "$PREFIX/share/gravity-particles"
    say "Готово. Ваши настройки остались в ~/.config/gravity-particles"
    exit 0
fi

say "Устанавливаю необходимые пакеты"
if command -v apt-get >/dev/null; then
    # Ошибка в чужом репозитории (например, устаревший ключ) не должна мешать установке.
    sudo apt-get update || echo "Предупреждение: не все списки пакетов обновились, продолжаю"
    sudo apt-get install -y build-essential cmake libglfw3-dev libglew-dev libfreetype-dev libharfbuzz-dev
elif command -v dnf >/dev/null; then
    sudo dnf install -y gcc-c++ cmake glfw-devel glew-devel freetype-devel harfbuzz-devel
elif command -v pacman >/dev/null; then
    sudo pacman -S --needed --noconfirm base-devel cmake glfw glew freetype2 harfbuzz
elif command -v zypper >/dev/null; then
    sudo zypper install -y gcc-c++ cmake glfw-devel glew-devel freetype2-devel harfbuzz-devel
else
    die "не удалось определить менеджер пакетов. Установите вручную: компилятор C++, cmake, glfw, glew, freetype, harfbuzz"
fi

say "Собираю программу (это займёт около минуты)"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DGP_BUILD_TESTS=OFF
cmake --build "$BUILD" -j "$(nproc 2>/dev/null || echo 2)"

say "Устанавливаю в $PREFIX"
cmake --install "$BUILD"

DESKTOP="$PREFIX/share/applications/gravity-particles.desktop"
if [[ -f "$DESKTOP" ]]; then
    # Полный путь — ярлык работает, даже если ~/.local/bin нет в PATH.
    sed -i "s|^Exec=.*|Exec=$PREFIX/bin/gravity_particles|" "$DESKTOP"
    command -v update-desktop-database >/dev/null && update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
fi

say "Готово!"
echo "Запуск: найдите «Гравитационные частицы» в меню приложений"
echo "или выполните в терминале: $PREFIX/bin/gravity_particles"
