#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR="${1:-"/tmp/aeld"}"
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR="$(realpath "$(dirname "$0")")"
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

echo "Using directory ${OUTDIR:?} for output"
mkdir -p "${OUTDIR:?}"

cd "$OUTDIR"
if [ ! -d "${OUTDIR:?}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR:?}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e "${OUTDIR:?}/linux-stable/arch/${ARCH}/boot/Image" ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    echo "Cleaning and configuring kernel tree"
    # make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" mrproper
    make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" defconfig
    echo "Building kernel"
    make -j "$(nproc --all)" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" all
fi

echo "Adding the Image in outdir"
cp "${OUTDIR:?}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR:?}/Image"

echo "Creating the staging directory for the root filesystem"
if [ -d "${OUTDIR:?}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR:?}/rootfs and starting over"
    sudo rm -rf "${OUTDIR:?}/rootfs"
fi

echo "Creating rootfs"
mkdir -p "${OUTDIR:?}/rootfs/"{bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr,usr/bin,usr/lib,usr/sbin,var,var/log}

if [ ! -d "${OUTDIR:?}/busybox" ]
then
git clone git://busybox.net/busybox.git
    pushd busybox &>/dev/null
    git checkout ${BUSYBOX_VERSION}
    make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" distclean
    make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" defconfig
else
    pushd busybox &>/dev/null
fi
echo "Building & installing busybox"
make -j "$(nproc --all)" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}"
make -j "$(nproc --all)" CONFIG_PREFIX="${OUTDIR:?}/rootfs" ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" install
popd &>/dev/null

# Make device nodes
echo "Creating dev nodes"
sudo mknod -m 666 "${OUTDIR:?}/rootfs/dev/null" c 1 3
sudo mknod -m 620 "${OUTDIR:?}/rootfs/dev/console" c 5 1

# Clean and build the writer utility
pushd "${FINDER_APP_DIR}" &>/dev/null
echo "Building & installing writer util"
make CROSS_COMPILE="${CROSS_COMPILE}" clean
make -j "$(nproc --all)" CROSS_COMPILE="${CROSS_COMPILE}"
# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
make -j "$(nproc --all)" CROSS_COMPILE="${CROSS_COMPILE}" DESTDIR="${OUTDIR:?}/rootfs/home" install
popd &>/dev/null

echo "Installing library dependencies"
# I feel like there's a programmatic way to ask gcc about this, but I'm lazy
SHLIBS=($("${CROSS_COMPILE}objdump" -p "${OUTDIR:?}/rootfs/bin/busybox" | grep NEEDED | awk '{print $2}')) 
SHLIBS+=($("${CROSS_COMPILE}objdump" -p "${OUTDIR:?}/rootfs/bin/busybox" | grep NEEDED | awk '{print $2}')) 
INTERPR="$("${CROSS_COMPILE}readelf" -a "${OUTDIR:?}/rootfs/bin/busybox" | grep -o "ld-.*.so.[0-9]*")"  # this is the same for both

# Add library dependencies to rootfs
install "$("${CROSS_COMPILE}gcc" -print-file-name="${INTERPR}")" "${OUTDIR:?}/rootfs/lib/"
for LIB in "${SHLIBS[@]}"; do
    install "$("${CROSS_COMPILE}gcc" -print-file-name="${LIB}")" "${OUTDIR:?}/rootfs/lib64/"
done

# Chown the root directory
sudo chown -R root:root "${OUTDIR:?}/rootfs"

# Create initramfs.cpio.gz
pushd "${OUTDIR:?}/rootfs" &>/dev/null
echo "Creating initramfs"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR:?}/initramfs.cpio"
popd &>/dev/null
gzip -f "${OUTDIR:?}/initramfs.cpio"