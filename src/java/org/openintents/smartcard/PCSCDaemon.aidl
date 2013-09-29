package org.openintents.smartcard;

interface PCSCDaemon {
    boolean start();
    void stop();
    int getFileDescriptor();
}
