#!/bin/bash
xhost +
USER_UID=$(id -u)
docker run -it --rm --ipc host \
  --device=/dev/dri \
  --group-add video \
  --volume=/tmp/.X11-unix:/tmp/.X11-unix \
  --volume=`realpath ../../..`:/project \
  --env="DISPLAY=$DISPLAY" \
  --device=/dev/snd \
  --volume=/run/user/${USER_UID}/pulse:/run/user/1000/pulse \
  --env=PULSE_SERVER=unix:$XDG_RUNTIME_DIR/pulse/native \
  ubuntu \
  /bin/bash -c "apt update -y \
    && apt install -y scons libfox-1.6-dev libxrandr2 \
    && /bin/bash"
xhost -
