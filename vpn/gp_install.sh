#!/bin/bash

# Determine command type
cmd_type="ui"
if [ $# -gt 0 ]; then
    case $1 in
        --cli-only)
            cmd_type="cli-only";;
        --arm)
            cmd_type="arm";;
        --help)
	    ;&
        *)
            cmd_type="usage";;
    esac
fi

# Determine Linux Distro and Version
. /etc/os-release

if [ $ID == "ubuntu" ]; then
    linux_ver=${VERSION_ID:0:2}
elif [[ $ID == "rhel" || $ID == "centos" ]]; then
    linux_ver=${VERSION_ID:0:1}
fi

case $cmd_type in
    cli-only)
        # CLI Only Install
        case $ID in
            ubuntu)
                case $linux_ver in
                    14)
                        ;&
                    16)
                        ;&
                    18)
                        apt-get install ./GlobalProtect_deb-*.deb;;
                     *)
                        apt-get install ./GlobalProtect_focal_deb-*.deb;;
	        esac
                ;;
            rhel)
                ;&
            centos)
                # Check if old GP package installed
                yum_output=$(yum list installed | grep globalprotect)
                if [[ $yum_output == *"globalprotect.x86"* ]]; then
                    echo "Older globalprotect detected...uninstalling..."
                    yum -y remove globalprotect
                fi
                
                case $linux_ver in
                    7)
                        yum -y install ./GlobalProtect_rpm-*;;
		    *)
                        yum -y install ./GlobalProtect_focal_rpm-*;;
                esac
	        ;;
            *)
                echo "Error: Unsupported Linux Distro: $ID"
	        exit
                ;;
        esac
        ;;

    arm)
        # ARM Install
        case $ID in
            ubuntu)
                case $linux_ver in
                    14)
                        ;&
                    16)
                        ;&
                    18)
                        apt-get install ./GlobalProtect_deb_arm-*.deb;;
		    *)
                        apt-get install ./GlobalProtect_focal_deb_arm-*.deb;;
	        esac
                ;;
            rhel)
                ;&
            centos)
                # Check if old GP package installed
                yum_output=$(yum list installed | grep globalprotect)
                if [[ $yum_output == *"globalprotect_arm.x86"* ]]; then
                    echo "Older globalprotect detected...uninstalling..."
                    yum -y remove globalprotect_arm
                fi
                
                case $linux_ver in
                    7)
                        yum -y install ./GlobalProtect_rpm_arm-*;;
		    *)
                        yum -y install ./GlobalProtect_focal_rpm_arm-*;;
	            esac
	            ;;
            *)
                echo "Error: Unsupported Linux Distro: $ID"
	        exit
                ;;
        esac
        ;;

    ui)
        # UI Install
        case $ID in
            ubuntu)
                case $linux_ver in
                    14)
                        ;&
                    16)
                        ;&
                    18)
                        apt-get install ./GlobalProtect_UI_deb-*.deb;;
		    *)
                        apt-get install ./GlobalProtect_UI_focal_deb-*.deb;;
	        esac
                ;;
            rhel)
                ;&
            centos)
                # Check if old GP package installed
                yum_output=$(yum list installed | grep globalprotect)
                if [[ $yum_output == *"globalprotect_UI.x86"* ]]; then
                    echo "Older globalprotect detected...uninstalling..."
                    yum -y remove globalprotect_UI
                fi
            
                # RHEL Package Dependencies
                if [ "$ID" = "centos" ]; then
                    yum -y install epel-release
                elif [ "$ID" = "rhel" ]; then
                    if [ "$linux_ver" = "7" ]; then
                        yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
                    elif [ "$linux_ver" = "8" ]; then
                        yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
                    elif [ "$linux_ver" = "9" ]; then
                        yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm
                    else
                        echo "Error: Unsupported RHEL version: $linux_ver"
			exit
                    fi
                else
                    echo "Error: Unrecognized OS: $ID"
                    exit
                fi

                echo "yum: Installing Qt5 WebKit and wmctrl..."
                yum -y install qt5-qtwebkit wmctrl

                # Install
                case $linux_ver in
                    7)
                        yum -y install ./GlobalProtect_UI_rpm-*
                        ;;
		    *)
                        yum -y install ./GlobalProtect_UI_focal_rpm-*;;
	        esac
                ;;
            *)
                echo "Error: Unsupported Linux Distro: $ID";;
        esac
        ;;
    usage)
        ;&
    *)
        echo "Usage: $ sudo ./gp_install [--cli-only | --arm | --help]"
        echo "  --cli-only: CLI Only"
        echo "  --arm:      ARM"
        echo "  no options: UI"
        ;;	
esac
