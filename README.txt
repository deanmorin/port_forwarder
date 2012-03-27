SETUP

First run the setup.sh script for either Fedora or OSX. 

Run 'make' to build the binaries, or 'make debug' to build binaries that display
more information to stdout.


USING THE PROGRAM

Edit 'forward.csv' to change the forwarding rules. They should be entered in the
form: 

forwarderPort,hostAddress,hostPort

You can choose which event base to use with switches. By default the best base
for the system will be used.
