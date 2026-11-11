#import "SamplerView.h"
#import "Backend.h"
#import <QuartzCore/QuartzCore.h>
#include <algorithm>
#include <vector>
#include "Host.h"

// The UI is laid out in units for a landscape screen 400 high and at least 860 wide. It is scaled to fit between the safe-area insets
// (notch / Dynamic Island, home indicator); what a taller screen leaves over goes to the timeline up to kMaxUnitsHigh, the rest is margin.
static const CGFloat kUnitsHigh = 400, kMinUnitsWide = 860, kMaxUnitsHigh = 440;

// colours are 0xAARRGGBB
static UIColor* colorOf(uint32_t argb) {
    return [UIColor colorWithRed:((argb >> 16) & 255) / 255.0 green:((argb >> 8) & 255) / 255.0 blue:(argb & 255) / 255.0 alpha:((argb >> 24) & 255) / 255.0];
}
static void setFill(CGContextRef c, uint32_t argb) {
    CGContextSetRGBFillColor(c, ((argb >> 16) & 255) / 255.0, ((argb >> 8) & 255) / 255.0, (argb & 255) / 255.0, ((argb >> 24) & 255) / 255.0);
}
static void setStroke(CGContextRef c, uint32_t argb) {
    CGContextSetRGBStrokeColor(c, ((argb >> 16) & 255) / 255.0, ((argb >> 8) & 255) / 255.0, (argb & 255) / 255.0, ((argb >> 24) & 255) / 255.0);
}

@implementation FMSamplerView {
    FMBackend* _backend;
    CADisplayLink* _link;
    std::vector<UiOp> _ops;
    CGFloat _scale;            // points per layout unit
    CGPoint _origin;           // where unit (0, 0) is, in points
    CGSize _units;             // size of the layout in units
    UITouch* _touch;           // the one finger the UI follows
    NSMutableDictionary<NSNumber*, UIFont*>* _fonts;
}

- (instancetype)initWithBackend:(FMBackend*)backend {
    if ((self = [super initWithFrame:CGRectZero])) {
        _backend = backend;
        _scale = 1;
        _units = CGSizeMake(kMinUnitsWide, kUnitsHigh);
        _fonts = [NSMutableDictionary dictionary];
        self.opaque = YES;
        self.backgroundColor = UIColor.whiteColor;
        self.contentMode = UIViewContentModeRedraw;
        self.multipleTouchEnabled = NO;
        self.exclusiveTouch = YES;
    }
    return self;
}

- (void)didMoveToWindow {
    [super didMoveToWindow];
    [_link invalidate];
    _link = nil;
    if (self.window) {   // redraw on every display refresh while on screen (the link stops by itself in the background)
        _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(step:)];
        _link.preferredFramesPerSecond = 60;
        [_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    }
}
- (void)step:(CADisplayLink*)link { [self setNeedsDisplay]; }

- (void)updateLayout {
    const CGRect b = self.bounds;
    const UIEdgeInsets safe = self.safeAreaInsets;
    const CGFloat w = std::max<CGFloat>(1, b.size.width - safe.left - safe.right), h = std::max<CGFloat>(1, b.size.height - safe.top - safe.bottom);
    _scale = std::min(h / kUnitsHigh, w / kMinUnitsWide);
    _units = CGSizeMake(w / _scale, std::min(h / _scale, kMaxUnitsHigh));
    _origin = CGPointMake(safe.left, safe.top + (h - _units.height * _scale) / 2);
}

- (UIFont*)fontOfSize:(CGFloat)size bold:(BOOL)bold {
    NSNumber* key = @((long)lround(size * 16) * 2 + (bold ? 1 : 0));
    UIFont* f = _fonts[key];
    if (!f) {   // a monospaced face 0.6 em wide: the layout counts on that (SamplerUI::fit)
        f = [UIFont fontWithName:bold ? @"Menlo-Bold" : @"Menlo-Regular" size:size];
        if (!f) f = [UIFont monospacedSystemFontOfSize:size weight:bold ? UIFontWeightBold : UIFontWeightRegular];
        _fonts[key] = f;
    }
    return f;
}

static void addRoundRect(CGContextRef c, CGRect r, CGFloat radius) {
    radius = std::max<CGFloat>(0, std::min(radius, std::min(r.size.width, r.size.height) / 2));
    if (radius <= 0) { CGContextAddRect(c, r); return; }
    CGPathRef p = CGPathCreateWithRoundedRect(r, radius, radius, NULL);
    CGContextAddPath(c, p);
    CGPathRelease(p);
}

- (void)drawRect:(CGRect)rect {
    CGContextRef c = UIGraphicsGetCurrentContext();
    if (!c) return;
    [self updateLayout];
    _backend.host->uiFrame((float)_units.width, (float)_units.height, [_backend nowMs], _ops);

    // the margins around the layout take the colour of its background (the first op)
    uint32_t bg = 0xffffffff;
    if (!_ops.empty() && _ops[0].type == UiOp::RRECT) bg = _ops[0].color | 0xff000000;
    setFill(c, bg);
    CGContextFillRect(c, self.bounds);

    CGContextSaveGState(c);
    CGContextTranslateCTM(c, _origin.x, _origin.y);
    CGContextScaleCTM(c, _scale, _scale);
    CGContextClipToRect(c, CGRectMake(0, 0, _units.width, _units.height));
    CGContextSetLineCap(c, kCGLineCapRound);
    int saves = 0;
    for (const UiOp& o : _ops) {
        const CGFloat f0 = o.f[0], f1 = o.f[1], f2 = o.f[2], f3 = o.f[3], f4 = o.f[4], f5 = o.f[5];
        switch (o.type) {
        case UiOp::RRECT:   // x y w h r
            if (f2 <= 0 || f3 <= 0) break;
            setFill(c, o.color);
            addRoundRect(c, CGRectMake(f0, f1, f2, f3), f4);
            CGContextFillPath(c);
            break;
        case UiOp::STROKE:  // x y w h r width
            if (f2 <= 0 || f3 <= 0) break;
            setStroke(c, o.color);
            CGContextSetLineWidth(c, f5 > 0 ? f5 : 1 / _scale);
            addRoundRect(c, CGRectMake(f0, f1, f2, f3), f4);
            CGContextStrokePath(c);
            break;
        case UiOp::TEXT: {  // x baseline size align(0 left, 1 centre, 2 right) bold
            if (o.text.empty() || f2 <= 0) break;
            NSString* s = [NSString stringWithUTF8String:o.text.c_str()];
            if (!s) break;
            UIFont* font = [self fontOfSize:f2 bold:f4 != 0];
            NSDictionary* attrs = @{NSFontAttributeName: font, NSForegroundColorAttributeName: colorOf(o.color)};
            const int align = (int)f3;
            CGFloat x = f0;
            if (align) { const CGFloat w = [s sizeWithAttributes:attrs].width; x -= align == 1 ? w / 2 : w; }
            [s drawAtPoint:CGPointMake(x, f1 - font.ascender) withAttributes:attrs];
            break;
        }
        case UiOp::LINE:    // x1 y1 x2 y2 width
            setStroke(c, o.color);
            CGContextSetLineWidth(c, f4 > 0 ? f4 : 1 / _scale);
            CGContextMoveToPoint(c, f0, f1);
            CGContextAddLineToPoint(c, f2, f3);
            CGContextStrokePath(c);
            break;
        case UiOp::CIRCLE:  // cx cy r
            if (f2 <= 0) break;
            setFill(c, o.color);
            CGContextFillEllipseInRect(c, CGRectMake(f0 - f2, f1 - f2, 2 * f2, 2 * f2));
            break;
        case UiOp::TRI:     // x1 y1 x2 y2 x3 y3
            setFill(c, o.color);
            CGContextMoveToPoint(c, f0, f1);
            CGContextAddLineToPoint(c, f2, f3);
            CGContextAddLineToPoint(c, f4, f5);
            CGContextClosePath(c);
            CGContextFillPath(c);
            break;
        case UiOp::CLIP:    // x y w h
            CGContextSaveGState(c); saves++;
            CGContextClipToRect(c, CGRectMake(f0, f1, std::max<CGFloat>(0, f2), std::max<CGFloat>(0, f3)));
            break;
        case UiOp::ENDCLIP:
            if (saves > 0) { CGContextRestoreGState(c); saves--; CGContextSetLineCap(c, kCGLineCapRound); }
            break;
        }
    }
    while (saves-- > 0) CGContextRestoreGState(c);
    CGContextRestoreGState(c);
}

// ---- touches: one finger, in layout units ----
- (void)sendTouch:(UITouch*)t action:(int)action {
    const CGPoint p = [t locationInView:self];
    _backend.host->uiTouch(action, (float)((p.x - _origin.x) / _scale), (float)((p.y - _origin.y) / _scale), [_backend nowMs]);
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (_touch) return;
    _touch = touches.anyObject;
    [self sendTouch:_touch action:0];
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (_touch && [touches containsObject:_touch]) [self sendTouch:_touch action:1];
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (_touch && [touches containsObject:_touch]) { [self sendTouch:_touch action:2]; _touch = nil; }
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self touchesEnded:touches withEvent:event];
}
@end
