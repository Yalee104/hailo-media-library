#!/bin/bash

USER=root
SERVER="root@10.0.0.1"
HOST=10.0.0.1
PASS=root

# Deploy the main application
echo "*****Copying application frontend_example to H15 /home/root/"
sshpass -p "$PASS" scp build/api/frontend_example "$USER"@"$HOST":/home/root/
sshpass -p "$PASS" scp build/api/jpeg_frontend_example "$USER"@"$HOST":/home/root/

# Deploy the json config files
echo ""
echo "*****Copying json config files to H15 /home/root/ must be on the same directory as frontend_example application"
sshpass -p "$PASS" scp api/examples/*.json "$USER"@"$HOST":/home/root/

# Deploy the library files
echo ""
echo "*****Copying/Replacing all library files to H15"
sshpass -p "$PASS" scp build/api/libhailo_medialibrary_api.* "$USER"@"$HOST":/usr/lib/
sshpass -p "$PASS" scp build/gst/libgstmedialibutils.* "$USER"@"$HOST":/usr/lib/
sshpass -p "$PASS" scp build/gst/libgstmedialib.* "$USER"@"$HOST":/usr/lib/gstreamer-1.0/
sshpass -p "$PASS" scp build/media_library/libhailo_* "$USER"@"$HOST":/usr/lib/


# Deploy the native full visualized inference app example
###########################################################################
echo ""
echo "*****Deploying native full visualized inference application to H15 under /home/root/custom_full_inference_app"
dir="/home/root/custom_full_inference_app"
dir_resource="/home/root/custom_full_inference_app/resources"

# Run the command on the remote server
sshpass -p $PASS ssh $SERVER << EOF

# Check if the directory exists
if [ -d "$dir" ]
then
    echo "Directory $dir already exists."
else
    echo "Directory $dir does not exist. Creating now."
    mkdir -p $dir
    echo "Directory $dir created."
fi

# Check if the resources directory exists
if [ -d "$dir_resource" ]
then
    echo "Directory $dir_resource already exists."
else
    echo "Directory $dir_resource does not exist. Creating now."
    mkdir -p $dir_resource
    echo "Directory $dir_resource created."
fi

EOF

sshpass -p "$PASS" scp -r build/api/custom_full_inference_example "$USER"@"$HOST":"$dir"
sshpass -p "$PASS" scp -r api/examples/native_custom/*.json "$USER"@"$HOST":"$dir"
sshpass -p "$PASS" scp -r api/examples/native_custom/Resources/*.hef "$USER"@"$HOST":"$dir_resource"



echo ""
echo "All done, please ignore message 'not a regular file' for files with .p extension"

