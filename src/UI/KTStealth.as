// Owned AS2 adapter. The light-gem drawings come from the user's local donor;
// none of the donor's ActionScript, game bindings or external movies is loaded.
stop();
_root.version = 15;
_root.lastTick = -1;
_root.lastPaint = getTimer();
_root.hasValue = false;
_root.hadV = false;
_root.panel._visible = false;
_root.panel.gem._visible = false;
_root.tabChildren = false;

function kt_paint() {
    _root.panel.gem.dark._alpha = 100 * (1 - _root.shownV);
    _root.panel.gem.light._alpha = 100 * _root.shownV;
}

function cry_onHide() {
    _root.panel._visible = false;
    _root.hasValue = false;
    _root.hadV = false;
}

function cry_onShow() {
    cry_onHide();
}

// The exact receipt distinguishes a real ActionScript call from native failure.
function kt_update(v, vk) {
    if (typeof(v) != "number" || !isFinite(v)) {
        _root.panel._visible = false;
        _root.hasValue = false;
        return "invalid";
    }
    _root.targetV = Math.max(0, Math.min(1, v));
    var now = getTimer();
    var expired = !_root.hasValue || now < _root.lastTick || now - _root.lastTick > 350;
    if (expired || !_root.hadV) {
        _root.shownV = _root.targetV;
    }
    _root.hadV = vk == true;
    _root.hasValue = true;
    _root.lastTick = now;
    _root.panel.gem._visible = vk == true;
    _root.panel._visible = vk == true;
    kt_paint();
    return "kt15";
}

_root.onEnterFrame = function() {
    var now = getTimer();
    if (!_root.hasValue || now < _root.lastTick || now - _root.lastTick > 350) {
        cry_onHide();
    } else {
        var fraction = Math.min(1, Math.max(0, now - _root.lastPaint) / 120);
        _root.shownV += (_root.targetV - _root.shownV) * fraction;
        kt_paint();
    }
    _root.lastPaint = now;
};
