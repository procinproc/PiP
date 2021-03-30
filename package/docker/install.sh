#!/bin/sh

set -x

case `uname -p` in
x86_64)  ARCH=amd64;;
aarch64) ARCH=arm64;;
*)	echo >&2 "Unknown processor type: `uname -p`"
	exit 1;;
esac

name="pip-${PIP_VERSION}-${DISTRO}-${ARCH}-rpm"
per_page=100

base_url="https://api.github.com/repos/${GITHUB_REPOSITORY}/actions/artifacts"
api_url="${base_url}?per_page=${per_page}"

yum -y install epel-release &&
yum -y install --enablerepo=epel jq &&
artifact_url=$(curl "$api_url" |
	jq -r '.artifacts | sort_by(.created_at) | reverse |
		map(select(.name == "'${name}'")) |
		.[0] | .archive_download_url') &&
curl -o artifact.zip -L -H "Authorization: token $PIP_BUILD_TOKEN" \
	"$artifact_url" &&
unzip artifact.zip &&
echo "====" &&
ls -alR &&
echo "====" &&
rpm -Uvh */*.rpm &&
rm -rf * &&
echo "====" &&
ls -alR &&
echo "===="
