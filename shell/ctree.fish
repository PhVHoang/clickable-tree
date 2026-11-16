# Source this file from ~/.config/fish/config.fish after installing ctree.
function ctree --description 'Interactive directory tree'
    for arg in $argv
        switch $arg
            case --help --version --plain
                command ctree $argv
                return $status
        end
    end
    if not test -t 0; or not test -t 1
        command ctree $argv
        return $status
    end
    set -l selected (command ctree --emit-cd $argv | string split0)
    if set -q selected[1]
        cd -- "$selected"
    end
end
