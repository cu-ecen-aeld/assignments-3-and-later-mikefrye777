#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE mrproper
    echo "clean done"
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE defconfig
    echo "config done"
    make -j4 ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE all
    echo "build done"
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE modules
    echo "modules done"
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE dtbs
    echo "DTBs done"

fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs
cd rootfs
touch init
mkdir bin dev etc home lib proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE CONFIG_PREFIX=${OUTDIR}/rootfs install
cd "${OUTDIR}/rootfs"

echo "Library dependencies"
pwd; ls
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$( ${CROSS_COMPILE}gcc -print-sysroot)
# ld-linux-aarch64.so.1 points into /lib64 directory from /lib
#cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
#cp -a ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib
cp -a ${SYSROOT}/lib64/libc-2.31.so ${OUTDIR}/rootfs/lib
#cp -a ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib
cp -a ${SYSROOT}/lib64/libm-2.31.so ${OUTDIR}/rootfs/lib
cp -a ${SYSROOT}/lib64/ld-2.31.so ${OUTDIR}/rootfs/lib
cp -a ${SYSROOT}/lib64/libresolv-2.31.so ${OUTDIR}/rootfs/lib
# Maybe I should just retain the /lib and /lib64 distinction? there was no mention of lib64 in the lectures
ln -s lib lib64
cd lib && \
	ln -s ld-2.31.so ld-linux-aarch64.so.1 && \
	ln -s libc-2.31.so libc.so.6 && \
	ln -s libm-2.31.so libm.so.6 && \
	ln -s libresolv-2.31.so libresolv.so.2

# TODO: Make device nodes
cd "${OUTDIR}/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/char0 c 5 1

# TODO: Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=$CROSS_COMPILE

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -LR ${FINDER_APP_DIR}/conf ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/finder.sh ${FINDER_APP_DIR}/writer ${FINDER_APP_DIR}/finder-test.sh ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
cd "${OUTDIR}/rootfs"
sudo chown -R root:root *


# TODO: Create initramfs.cpio.gz
find . | cpio -o -H newc | gzip > ../initramfs.cpio.gz
