package com.khiproteam.khipro;

import android.inputmethodservice.InputMethodService;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public final class JavaImeExample extends InputMethodService {
    private KhiproIme khipro;

    @Override
    public void onStartInput(EditorInfo info, boolean restarting) {
        khipro = new KhiproIme(getCurrentInputConnection(), KhiproVariant.TOUCHSCREEN);
    }

    @Override
    public void onFinishInput() {
        if (khipro != null) {
            khipro.close();
            khipro = null;
        }
    }

    public void onKeyPress(String latinKey) {
        InputConnection ic = getCurrentInputConnection();
        if (khipro == null || ic == null) return;
        if (!khipro.onKey(latinKey)) {
            ic.commitText(latinKey, 1);
        }
    }

    public void onBackspace() {
        InputConnection ic = getCurrentInputConnection();
        if (khipro == null || ic == null) return;
        if (!khipro.onBackspace()) {
            ic.deleteSurroundingText(1, 0);
        }
    }

    public void onEnter() {
        if (khipro != null) khipro.commit();
    }
}
