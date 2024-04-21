An expansion for DSLinux

Install packages from the www/ folder by running...

```
tar -xvf package.tar -C / 
```

You can run coreutils-init.sh to copy files from coreutils to the /usr/bin/ folder, which enables some useful commands like `wc`. `base64`, and `bc`. However, the developers probably did not do this because some are buggy, such as `printf` which just seems to crash. Inside of the www/ folder you may find some redundant packages that are also in coreutils. Try one of these redundant ports if one of the coreutils ones are buggy.

## Lua

The `lua.tar` contains both the Lua5 interpreter and compiler compiled with POSIX support. DSLinux by default has no methodology, outside of shell scripts, to write software on the console itself. This allows for writing Lua code and compiling it and running it on the NDS itself, and it runs quite fast.

## Dropbear

Probably the most useful thing in this repository is an updated version of `ssh` and `scp` which is found in the `dropbear.tar` package. This allows you to SSH into a modern device without having to downgrade your security in the `/etc/ssh/sshd_config` settings.

## WGET

The version of `wget` here is a later version but without SSL support (meaning HTTPS websites don't work). The version of `wget` in DSLinux by default has SSL support but it is outdated and so it fails to form an SSL connection on many websites. The SSL code for the later version of `wget` seems to build but crash when actually used, so it is built without SSL support. You can choose either to stick with the older version of `wget` with finnicky SSL support, or the newer one without it, depending on your preference.

## AWK

Another useful command ported here is `awk`, which is itself an entire scripting language, but typically used for processing output of other commands in shel scripts. 
