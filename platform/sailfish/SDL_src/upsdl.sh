#/bin/bash

_pwd=`dirname $(readlink -e "$0")`
_base_path="$_pwd"
_src_path=$1

function checkdir() 
{
    local current_dir="${_base_path}$1"
    local each=""
    echo "Check dir: $current_dir"
    # echo "entry list: \"$(ls $current_dir|sed ':a;N;$!ba;s/\n/ /g')\""
    for each in $(ls $current_dir) ; do
        if [ -f $current_dir/$each ] ; then
            # echo "Check file: $each"
            if [ -f ${_src_path}${1}/${each} ] ; then
                echo -n "Replace file \"${1}/$each\" :"
                rsync -avP ${_src_path}${1}/${each} ${current_dir}/${each}
                if [ $? -ne 0 ] ; then
                    echo "FAIL"
                else
                    echo "DONE"
                fi
            fi
        elif [ -d $current_dir/$each ] ; then
            #echo "Check dir: $each"
            checkdir "$1/$each"
        fi
    done
    return 0
}

echo "src path: $_src_path"
checkdir ""