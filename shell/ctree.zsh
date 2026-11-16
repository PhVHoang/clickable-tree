# Source this file from ~/.zshrc after installing the ctree executable.
ctree() {
    local arg selected output rc
    for arg in "$@"; do
        case "$arg" in
            --help|--version|--plain) command ctree "$@"; return $? ;;
        esac
    done
    if [[ ! -t 0 || ! -t 1 ]]; then
        command ctree "$@"
        return $?
    fi
    output=$(mktemp "${TMPDIR:-/tmp}/ctree.XXXXXX") || return 1
    command ctree --emit-cd "$@" >| "$output"
    rc=$?
    if (( rc == 0 )) && IFS= read -r -d '' selected < "$output"; then
        builtin cd -- "$selected"
        rc=$?
    fi
    command rm -f -- "$output"
    return $rc
}
