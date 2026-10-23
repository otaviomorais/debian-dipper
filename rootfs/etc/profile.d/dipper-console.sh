# Optional local tty1 session. Never attach SSH, root or nested shells.
if [ -f /etc/dipper-console/autostart ] && [ -z "${TMUX:-}" ] &&
   [ -z "${SSH_CONNECTION:-}" ] && [ "$(id -u)" != 0 ]; then
    case $- in
        *i*) if [ "$(tty 2>/dev/null)" = /dev/tty1 ]; then
                 dipper-console
             fi ;;
    esac
fi
