#!/bin/bash

# -------------------------------------------------------------------------- #
# Copyright 2002-2017, OpenNebula Project, OpenNebula Systems                #
#                                                                            #
# Licensed under the Apache License, Version 2.0 (the "License"); you may    #
# not use this file except in compliance with the License. You may obtain    #
# a copy of the License at                                                   #
#                                                                            #
# http://www.apache.org/licenses/LICENSE-2.0                                 #
#                                                                            #
# Unless required by applicable law or agreed to in writing, software        #
# distributed under the License is distributed on an "AS IS" BASIS,          #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
# See the License for the specific language governing permissions and        #
# limitations under the License.                                             #
#--------------------------------------------------------------------------- #

###############################################################################
# This script is used to copy a VM image (SRC) to the image repository as DST
# Several SRC types are supported
###############################################################################

# -------- Set up the environment to source common tools & conf ------------

if [ -z "${ONE_LOCATION}" ]; then
    LIB_LOCATION=/usr/lib/one
else
    LIB_LOCATION=$ONE_LOCATION/lib
fi

. $LIB_LOCATION/sh/scripts_common.sh

DRIVER_PATH=$(dirname $0)
source ${DRIVER_PATH}/../libfs.sh

# -------- Get cp and datastore arguments from OpenNebula core ------------

DRV_ACTION=$1
ID=$2

export DRV_ACTION

UTILS_PATH="${DRIVER_PATH}/.."

XPATH="$UTILS_PATH/xpath.rb -b $DRV_ACTION"

unset i XPATH_ELEMENTS

while IFS= read -r -d '' element; do
    XPATH_ELEMENTS[i++]="$element"
done < <($XPATH     /DS_DRIVER_ACTION_DATA/DATASTORE/BASE_PATH \
                    /DS_DRIVER_ACTION_DATA/DATASTORE/TEMPLATE/RESTRICTED_DIRS \
                    /DS_DRIVER_ACTION_DATA/DATASTORE/TEMPLATE/SAFE_DIRS \
                    /DS_DRIVER_ACTION_DATA/DATASTORE/TEMPLATE/BRIDGE_LIST \
                    /DS_DRIVER_ACTION_DATA/DATASTORE/TEMPLATE/STAGING_DIR \
                    /DS_DRIVER_ACTION_DATA/DATASTORE/TYPE \
                    /DS_DRIVER_ACTION_DATA/IMAGE/PATH \
                    /DS_DRIVER_ACTION_DATA/IMAGE/TEMPLATE/MD5 \
                    /DS_DRIVER_ACTION_DATA/IMAGE/TEMPLATE/SHA1 \
                    /DS_DRIVER_ACTION_DATA/DATASTORE/TEMPLATE/NO_DECOMPRESS \
                    /DS_DRIVER_ACTION_DATA/DATASTORE/TEMPLATE/LIMIT_TRANSFER_BW)

unset i

BASE_PATH="${XPATH_ELEMENTS[i++]}"
RESTRICTED_DIRS="${XPATH_ELEMENTS[i++]}"
SAFE_DIRS="${XPATH_ELEMENTS[i++]}"
BRIDGE_LIST="${XPATH_ELEMENTS[i++]}"
STAGING_DIR="${XPATH_ELEMENTS[i++]:-/var/tmp}"
TYPE="${XPATH_ELEMENTS[i++]}"
SRC="${XPATH_ELEMENTS[i++]}"
MD5="${XPATH_ELEMENTS[i++]}"
SHA1="${XPATH_ELEMENTS[i++]}"
NO_DECOMPRESS="${XPATH_ELEMENTS[i++]}"
LIMIT_TRANSFER_BW="${XPATH_ELEMENTS[i++]}"

DST=`generate_image_path`
IMAGE_HASH=`basename $DST`

set_up_datastore "$BASE_PATH" "$RESTRICTED_DIRS" "$SAFE_DIRS"

# Disable auto-decompress for the 'files' datastores (type 2)
if [ "$TYPE" = "2" ]; then
    NO_DECOMPRESS="${NO_DECOMPRESS:-yes}"
fi

if [ -n "$BRIDGE_LIST" ]; then
    DOWNLOADER_ARGS=`set_downloader_args "$MD5" "$SHA1" "$NO_DECOMPRESS" "$LIMIT_TRANSFER_BW" "$SRC" -`
else
    DOWNLOADER_ARGS=`set_downloader_args "$MD5" "$SHA1" "$NO_DECOMPRESS" "$LIMIT_TRANSFER_BW" "$SRC" "$DST"`
fi

COPY_COMMAND="$UTILS_PATH/downloader.sh $DOWNLOADER_ARGS"

if echo "$SRC" | grep -vq '^https\?://'; then

#    if [ `check_restricted $SRC` -eq 1 ]; then
#        log_error "Not allowed to copy images from $RESTRICTED_DIRS"
#        error_message "Not allowed to copy image file $SRC"
#        exit -1
#    fi

    log "Copying local image $SRC to the image repository"
else
    log "Downloading image $SRC to the image repository"
fi

if [ -n "$BRIDGE_LIST" ]; then
    DST_HOST=`get_destination_host $ID`
    TMP_DST="$STAGING_DIR/$IMAGE_HASH"

    exec_and_log "eval $COPY_COMMAND | $SSH $DST_HOST $DD of=$TMP_DST bs=64k" \
                 "Error dumping $SRC to $DST_HOST:$TMP_DST"

    ssh_exec_and_log    "$DST_HOST" "mkdir -p $BASE_PATH; mv -f $TMP_DST $DST" \
                        "Error moving $TMP_DST to $DST in $DST_HOST"
else
    mkdir -p "$BASE_PATH"
    exec_and_log "$COPY_COMMAND" "Error copying $SRC to $DST"
fi

echo "$DST"
