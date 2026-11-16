# Source this file from ~/.bashrc after installing the ctree executable.
ctree() {
    local arg selected
    for arg in "$@"; do
        case "$arg" in
            --help|--version|--plain) command ctree "$@"; return $? ;;
        esac
    done
    if [[ ! -t 0 || ! -t 1 ]]; then
        command ctree "$@"
        return $?
    fi
    if IFS= read -r -d '' selected < <(command ctree --emit-cd "$@"); then
        builtin cd -- "$selected"
    fi
}
