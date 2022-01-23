package de.prosiebensat1digital.oasisjsbridge;

import de.prosiebensat1digital.oasisjsbridge.TestAidlParcelable;

interface TestAidlCallback {
    void onDone();
    void onDoneWithParcelable(in TestAidlParcelable p);
    void onDoneWithParcelableArray(in TestAidlParcelable[] pa);
}
