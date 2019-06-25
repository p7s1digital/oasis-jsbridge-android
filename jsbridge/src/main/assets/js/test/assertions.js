function fail(reason) {
    throw new Error(reason);
}

function assert(v, reason) {
    if (!v) {
        throw new Error(reason)
    }
}

function assertTrue(v, reason) {
    if (v != true) {
        throw new Error(reason)
    }
}

function assertFalse(v, reason) {
    if (v != false) {
        throw new Error(reason)
    }
}

function assertEquals(v1, v2, reason) {
    var str1 = JSON.stringify(v1);
    var str2 = JSON.stringify(v2);
    if (str1 !== str2) {
        var msg = reason ? reason : (
            "Values are not equal (received: " + str1 + ", expected: " + str2 + ")"
        );
        throw new Error(msg);
    }
}

function assertNull(v, reason) {
    if (v != null) {
        throw new Error(reason)
    }
}

function assertUndefined(v, reason) {
    if (v != undefined) {
        throw new Error(reason)
    }
}

function assertNonNullObject(v, reason) {
    if (typeof v !== "object" || v = null) {
        throw new Error(reason)
    }
}

function assertThrows(fn, reason) {
    var didThrow = false;
    try {
        fn();
    }
    catch (e) {
        didThrow = true;
    }

    if (!didThrow) {
        throw new Error(reason);
    }
}

function assertDoNotThrow(fn, reason) {
    var didThrow = false;
    try {
        fn();
    }
    catch (e) {
        didThrow = true;
    }

    if (didThrow) {
        throw new Error(reason);
    }
}
