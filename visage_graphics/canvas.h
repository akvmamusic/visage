/* Copyright Vital Audio, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "font.h"
#include "graphics_utils.h"
#include "layer.h"
#include "region.h"
#include "screenshot.h"
#include "shape_batcher.h"
#include "text.h"
#include "theme.h"
#include "visage_utils/space.h"
#include "visage_utils/time_utils.h"

namespace visage {
  class Palette;
  class Shader;

  class Canvas {
  public:
    static constexpr float kSqrt2 = 1.4142135623730950488016887242097f;
    static constexpr float kDefaultSquirclePower = 4.0f;

    static bool swapChainSupported();

    struct State {
      int x = 0;
      int y = 0;
      theme::OverrideId palette_override;
      const PackedBrush* brush = nullptr;
      ClampBounds clamp;
      BlendMode blend_mode = BlendMode::Alpha;
      Region* current_region = nullptr;
    };

    Canvas();
    Canvas(const Canvas& other) = delete;
    Canvas& operator=(const Canvas&) = delete;

    void clearDrawnShapes();
    int submit(int submit_pass = 0);

    void requestScreenshot();
    const Screenshot& screenshot() const;

    void ensureLayerExists(int layer);
    Layer* layer(int index) {
      ensureLayerExists(index);
      return layers_[index];
    }

    void invalidateRectInRegion(Bounds rect, const Region* region, int layer);
    void addToPackedLayer(Region* region, int layer_index);
    void removeFromPackedLayer(Region* region, int layer_index);
    void changePackedLayer(Region* region, int from, int to);

    void pairToWindow(void* window_handle, int width, int height) {
      VISAGE_ASSERT(swapChainSupported());
      composite_layer_.pairToWindow(window_handle, width, height);
      setDimensions(width, height);
    }

    void setWindowless(int width, int height) { composite_layer_.setHeadlessRender(width, height); }

    void removeFromWindow() { composite_layer_.removeFromWindow(); }

    void setDimensions(int width, int height);
    void setWidthScale(float width_scale) { width_scale_ = width_scale; }
    void setHeightScale(float height_scale) { height_scale_ = height_scale; }
    void setDpiScale(float scale) { dpi_scale_ = scale; }

    float widthScale() const { return width_scale_; }
    float heightScale() const { return height_scale_; }
    float dpiScale() const { return dpi_scale_; }
    void updateTime(double time);
    double time() const { return render_time_; }
    double deltaTime() const { return delta_time_; }
    int frameCount() const { return render_frame_; }

    void setBlendMode(BlendMode blend_mode) { state_.blend_mode = blend_mode; }
    void setBrush(const Brush& brush) {
      state_.brush = state_.current_region->addBrush(&gradient_atlas_, brush);
    }
    void setColor(const Brush& brush) { setBrush(brush); }
    void setColor(unsigned int color) { setBrush(Brush::solid(color)); }
    void setColor(const Color& color) { setBrush(Brush::solid(color)); }
    void setColor(theme::ColorId color_id) { setBrush(color(color_id)); }

    void setBlendedColor(theme::ColorId color_from, theme::ColorId color_to, float t) {
      setBrush(blendedColor(color_from, color_to, t));
    }

    void fill(float x, float y, float width, float height) {
      addShape(Fill(state_.clamp.clamp(x, y, width, height), state_.brush, state_.x + x,
                    state_.y + y, width, height));
    }

    void circle(float x, float y, float width) {
      addShape(Circle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width));
    }

    void fadeCircle(float x, float y, float width, float fade) {
      Circle circle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width);
      circle.pixel_width = fade;
      addShape(circle);
    }

    void ring(float x, float y, float width, float thickness) {
      Circle circle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width);
      circle.thickness = thickness;
      addShape(circle);
    }

    void squircle(float x, float y, float width, float power = kDefaultSquirclePower) {
      addShape(Squircle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, width, power));
    }

    void squircleBorder(float x, float y, float width, float power, float thickness) {
      Squircle squircle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, width, power);
      squircle.thickness = thickness;
      addShape(squircle);
    }

    void superEllipse(float x, float y, float width, float height, float power) {
      addShape(Squircle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height, power));
    }

    void roundedArc(float x, float y, float width, float thickness, float center_radians,
                    float radians, float pixel_width = 1.0f) {
      addShape(RoundedArc(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, width,
                          thickness + 1.0f, center_radians, radians));
    }

    void flatArc(float x, float y, float width, float thickness, float center_radians,
                 float radians, float pixel_width = 1.0f) {
      addShape(FlatArc(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, width,
                       thickness + 1.0f, center_radians, radians));
    }

    void arc(float x, float y, float width, float thickness, float center_radians, float radians,
             bool rounded = false, float pixel_width = 1.0f) {
      if (rounded)
        roundedArc(x, y, width, thickness, center_radians, radians, pixel_width);
      else
        flatArc(x, y, width, thickness, center_radians, radians, pixel_width);
    }

    void roundedArcShadow(float x, float y, float width, float thickness, float center_radians,
                          float radians, float shadow_width, bool rounded = false) {
      float full_width = width + 2.0f * shadow_width;
      RoundedArc arc(state_.clamp, state_.brush, state_.x + x - shadow_width,
                     state_.y + y - shadow_width, full_width, full_width,
                     thickness + 1.0f + 2.0f * shadow_width, center_radians, radians);
      arc.pixel_width = shadow_width;
      addShape(arc);
    }

    void flatArcShadow(float x, float y, float width, float thickness, float center_radians,
                       float radians, float shadow_width, bool rounded = false) {
      float full_width = width + 2.0f * shadow_width;
      FlatArc arc(state_.clamp, state_.brush, state_.x + x - shadow_width,
                  state_.y + y - shadow_width, full_width, full_width,
                  thickness + 1.0f + 2.0f * shadow_width, center_radians, radians);
      arc.pixel_width = shadow_width;
      addShape(arc);
    }

    void segment(float a_x, float a_y, float b_x, float b_y, float thickness, bool rounded = false,
                 float pixel_width = 1.0f) {
      float x = std::min(a_x, b_x) - thickness;
      float width = std::max(a_x, b_x) + thickness - x;
      float y = std::min(a_y, b_y) - thickness;
      float height = std::max(a_y, b_y) + thickness - y;

      float x1 = 2.0f * (a_x - x) / width - 1.0f;
      float y1 = 2.0f * (a_y - y) / height - 1.0f;
      float x2 = 2.0f * (b_x - x) / width - 1.0f;
      float y2 = 2.0f * (b_y - y) / height - 1.0f;

      if (rounded) {
        addShape(RoundedSegment(state_.clamp, state_.brush, state_.x + x, state_.y + y, width,
                                height, x1, y1, x2, y2, thickness + 1.0f, pixel_width));
      }
      else {
        addShape(FlatSegment(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height,
                             x1, y1, x2, y2, thickness + 1.0f, pixel_width));
      }
    }

    void quadratic(float a_x, float a_y, float b_x, float b_y, float c_x, float c_y,
                   float thickness, float pixel_width = 1.0f) {
      if (tryDrawCollinearQuadratic(a_x, a_y, b_x, b_y, c_x, c_y, thickness, pixel_width))
        return;

      float x = std::min(std::min(a_x, b_x), c_x) - thickness;
      float width = std::max(std::max(a_x, b_x), c_x) + thickness - x;
      float y = std::min(std::min(a_y, b_y), c_y) - thickness;
      float height = std::max(std::max(a_y, b_y), c_y) + thickness - y;

      float x1 = 2.0f * (a_x - x) / width - 1.0f;
      float y1 = 2.0f * (a_y - y) / height - 1.0f;
      float x2 = 2.0f * (b_x - x) / width - 1.0f;
      float y2 = 2.0f * (b_y - y) / height - 1.0f;
      float x3 = 2.0f * (c_x - x) / width - 1.0f;
      float y3 = 2.0f * (c_y - y) / height - 1.0f;

      addShape(QuadraticBezier(state_.clamp, state_.brush, state_.x + x, state_.y + y, width,
                               height, x1, y1, x2, y2, x3, y3, thickness + 1.0f, pixel_width));
    }

    void rectangle(float x, float y, float width, float height) {
      addShape(Rectangle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height));
    }

    void rectangleBorder(float x, float y, float width, float height, float thickness) {
      Rectangle border(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height);
      border.thickness = thickness + 1.0f;
      addShape(border);
    }

    void roundedRectangle(float x, float y, float width, float height, float rounding) {
      addShape(RoundedRectangle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width,
                                height, std::max(1.0f, rounding)));
    }

    void diamond(float x, float y, float width, float rounding) {
      addShape(Diamond(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, width,
                       std::max(1.0f, rounding)));
    }

    void leftRoundedRectangle(float x, float y, float width, float height, float rounding) {
      ClampBounds clamp = state_.clamp;
      clamp.right = std::min(clamp.right, state_.x + x + width);
      addShape(RoundedRectangle(clamp, state_.brush, state_.x + x, state_.y + y,
                                width + rounding + 1.0f, height, std::max(1.0f, rounding)));
    }

    void rightRoundedRectangle(float x, float y, float width, float height, float rounding) {
      ClampBounds clamp = state_.clamp;
      clamp.left = std::max(clamp.left, state_.x + x);
      float growth = rounding + 1.0f;
      addShape(RoundedRectangle(clamp, state_.brush, state_.x + x - growth, state_.y + y,
                                width + growth, height, std::max(1.0f, rounding)));
    }

    void topRoundedRectangle(float x, float y, float width, float height, float rounding) {
      ClampBounds clamp = state_.clamp;
      clamp.bottom = std::min(clamp.bottom, state_.y + y + height);
      addShape(RoundedRectangle(clamp, state_.brush, state_.x + x, state_.y + y, width,
                                height + rounding + 1.0f, std::max(1.0f, rounding)));
    }

    void bottomRoundedRectangle(float x, float y, float width, float height, float rounding) {
      ClampBounds clamp = state_.clamp;
      clamp.top = std::max(clamp.top, state_.y + y);
      float growth = rounding + 1.0f;
      addShape(RoundedRectangle(clamp, state_.brush, state_.x + x, state_.y + y - growth, width,
                                height + growth, std::max(1.0f, rounding)));
    }

    void rectangleShadow(float x, float y, float width, float height, float blur_radius) {
      if (blur_radius > 0.0f) {
        Rectangle rectangle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height);
        rectangle.pixel_width = blur_radius;
        addShape(rectangle);
      }
    }

    void roundedRectangleShadow(float x, float y, float width, float height, float rounding,
                                float blur_radius) {
      if (blur_radius <= 0.0f)
        return;

      float offset = -blur_radius * 0.5f;
      if (rounding <= 1.0f)
        rectangleShadow(state_.x + x + offset, state_.y + y + offset, width + blur_radius,
                        height + blur_radius, blur_radius);
      else {
        RoundedRectangle shadow(state_.clamp, state_.brush, state_.x + x + offset, state_.y + y + offset,
                                width + blur_radius, height + blur_radius, rounding);
        shadow.pixel_width = blur_radius;
        addShape(shadow);
      }
    }

    void roundedRectangleBorder(float x, float y, float width, float height, float rounding, float thickness) {
      saveState();
      float left = state_.clamp.left;
      float right = state_.clamp.right;
      float top = state_.clamp.top;
      float bottom = state_.clamp.bottom;

      float part = std::max(rounding, thickness);
      state_.clamp.right = std::min(right, state_.x + x + part + 1.0f);
      fullRoundedRectangleBorder(x, y, width, height, rounding, thickness);
      state_.clamp.right = right;
      state_.clamp.left = std::max(left, state_.x + x + width - part - 1.0f);
      fullRoundedRectangleBorder(x, y, width, height, rounding, thickness);

      state_.clamp.left = std::max(left, state_.x + x + part + 1.0f);
      state_.clamp.right = std::min(right, state_.x + x + width - part - 1.0f);
      state_.clamp.bottom = std::min(bottom, state_.y + y + part + 1.0f);
      fullRoundedRectangleBorder(x, y, width, height, rounding, thickness);
      state_.clamp.bottom = bottom;
      state_.clamp.top = std::max(top, state_.y + y + height - part - 1.0f);
      fullRoundedRectangleBorder(x, y, width, height, rounding, thickness);

      restoreState();
    }

    void triangleBorder(float a_x, float a_y, float b_x, float b_y, float c_x, float c_y, float thickness) {
      outerRoundedTriangleBorder(a_x, a_y, b_x, b_y, c_x, c_y, 0.0f, thickness);
    }

    void triangle(float a_x, float a_y, float b_x, float b_y, float c_x, float c_y) {
      float d1_x = a_x - b_x;
      float d1_y = a_y - b_y;
      float d2_x = a_x - c_x;
      float d2_y = a_y - c_y;
      float thickness = sqrtf(std::max(d1_x * d1_x + d1_y * d1_y, d2_x * d2_x + d2_y * d2_y));
      outerRoundedTriangleBorder(a_x, a_y, b_x, b_y, c_x, c_y, 0.0f, thickness);
    }

    void roundedTriangleBorder(float a_x, float a_y, float b_x, float b_y, float c_x, float c_y,
                               float rounding, float thickness) {
      float d_ab = sqrtf((a_x - b_x) * (a_x - b_x) + (a_y - b_y) * (a_y - b_y));
      float d_bc = sqrtf((b_x - c_x) * (b_x - c_x) + (b_y - c_y) * (b_y - c_y));
      float d_ca = sqrtf((c_x - a_x) * (c_x - a_x) + (c_y - a_y) * (c_y - a_y));
      float perimeter = d_ab + d_bc + d_ca;
      float inscribed_circle_x = (d_bc * a_x + d_ca * b_x + d_ab * c_x) / perimeter;
      float inscribed_circle_y = (d_bc * a_y + d_ca * b_y + d_ab * c_y) / perimeter;
      float s = perimeter * 0.5f;
      float inscribed_circle_radius = sqrtf(s * (s - d_ab) * (s - d_bc) * (s - d_ca)) / s;

      rounding = std::min(rounding, inscribed_circle_radius);
      float shrinking = rounding / inscribed_circle_radius;
      outerRoundedTriangleBorder(a_x + (inscribed_circle_x - a_x) * shrinking,
                                 a_y + (inscribed_circle_y - a_y) * shrinking,
                                 b_x + (inscribed_circle_x - b_x) * shrinking,
                                 b_y + (inscribed_circle_y - b_y) * shrinking,
                                 c_x + (inscribed_circle_x - c_x) * shrinking,
                                 c_y + (inscribed_circle_y - c_y) * shrinking, rounding, thickness);
    }

    void roundedTriangle(float a_x, float a_y, float b_x, float b_y, float c_x, float c_y, float rounding) {
      float d1_x = a_x - b_x;
      float d1_y = a_y - b_y;
      float d2_x = a_x - c_x;
      float d2_y = a_y - c_y;
      float thickness = sqrtf(std::max(d1_x * d1_x + d1_y * d1_y, d2_x * d2_x + d2_y * d2_y));
      roundedTriangleBorder(a_x, a_y, b_x, b_y, c_x, c_y, rounding, thickness);
    }

    void triangleLeft(float x, float y, float width) {
      float h = width * 2.0f;
      outerRoundedTriangleBorder(x + width, y, x + width, y + h, x, y + h * 0.5f, 0.0f, width);
    }

    void triangleRight(float x, float y, float width) {
      float h = width * 2.0f;
      outerRoundedTriangleBorder(x, y, x, y + h, x + width, y + h * 0.5f, 0.0f, width);
    }

    void triangleUp(float x, float y, float width) {
      float w = width * 2.0f;
      outerRoundedTriangleBorder(x, y + width, x + w, y + width, x + w * 0.5f, y, 0.0f, width);
    }

    void triangleDown(float x, float y, float width) {
      float w = width * 2.0f;
      outerRoundedTriangleBorder(x, y, x + w, y, x + w * 0.5f, y + width, 0.0f, width);
    }

    void text(Text* text, float x, float y, float width, float height, Direction dir = Direction::Up) {
      VISAGE_ASSERT(text->font().packedFont());
      TextBlock text_block(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height,
                           text, dir);
      addShape(std::move(text_block));
    }

    void text(const String& string, const Font& font, Font::Justification justification, float x,
              float y, float width, float height, Direction dir = Direction::Up) {
      if (!string.isEmpty()) {
        Text* stored_text = state_.current_region->addText(string, font, justification);
        text(stored_text, x, y, width, height, dir);
      }
    }

    void svg(const Svg& svg, float x, float y) {
      addShape(ImageWrapper(state_.clamp, state_.brush, state_.x + x, state_.y + y, svg.width,
                            svg.height, svg, imageAtlas()));
    }

    void svg(const char* svg_data, int svg_size, float x, float y, int width, int height,
             int blur_radius = 0) {
      svg({ svg_data, svg_size, width, height, blur_radius }, x, y);
    }

    void svg(const EmbeddedFile& file, float x, float y, int width, int height, int blur_radius = 0) {
      svg(file.data, file.size, x, y, width, height, blur_radius);
    }

    void image(const Image& image, float x, float y) {
      addShape(ImageWrapper(state_.clamp, state_.brush, state_.x + x, state_.y + y, image.width,
                            image.height, image, imageAtlas()));
    }

    void image(const char* image_data, int image_size, float x, float y, int width, int height) {
      image({ image_data, image_size, width, height }, x, y);
    }

    void image(const EmbeddedFile& image_file, float x, float y, int width, int height) {
      image(image_file.data, image_file.size, x, y, width, height);
    }

    void shader(Shader* shader, float x, float y, float width, float height) {
      addShape(ShaderWrapper(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height, shader));
    }

    void line(Line* line, float x, float y, float width, float height, float line_width) {
      addShape(LineWrapper(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height,
                           line, line_width));
    }

    void lineFill(Line* line, float x, float y, float width, float height, float fill_position) {
      addShape(LineFillWrapper(state_.clamp, state_.brush, state_.x + x, state_.y + y, width,
                               height, line, fill_position));
    }

    void saveState() { state_memory_.push_back(state_); }

    void restoreState() {
      state_ = state_memory_.back();
      state_memory_.pop_back();
    }

    void setPosition(int x, int y) {
      state_.x += x;
      state_.y += y;
    }

    void addRegion(Region* region) {
      default_region_.addRegion(region);
      region->setCanvas(this);
    }

    void beginRegion(Region* region) {
      region->clear();
      saveState();
      state_.x = 0;
      state_.y = 0;
      state_.brush = nullptr;
      state_.blend_mode = BlendMode::Alpha;
      setClampBounds(0, 0, region->width(), region->height());
      state_.current_region = region;
    }

    void endRegion() { restoreState(); }

    void setPalette(Palette* palette) { palette_ = palette; }
    void setPaletteOverride(theme::OverrideId override_id) {
      state_.palette_override = override_id;
    }

    void setClampBounds(int x, int y, int width, int height) {
      VISAGE_ASSERT(width >= 0);
      VISAGE_ASSERT(height >= 0);
      state_.clamp.left = state_.x + x;
      state_.clamp.top = state_.y + y;
      state_.clamp.right = state_.clamp.left + width;
      state_.clamp.bottom = state_.clamp.top + height;
    }

    void setClampBounds(const ClampBounds& bounds) { state_.clamp = bounds; }

    void trimClampBounds(int x, int y, int width, int height) {
      state_.clamp = state_.clamp.clamp(state_.x + x, state_.y + y, width, height);
    }

    void moveClampBounds(int x_offset, int y_offset) {
      state_.clamp.left += x_offset;
      state_.clamp.top += y_offset;
      state_.clamp.right += x_offset;
      state_.clamp.bottom += y_offset;
    }

    const ClampBounds& currentClampBounds() const { return state_.clamp; }
    bool totallyClamped() const { return state_.clamp.totallyClamped(); }

    int x() const { return state_.x; }
    int y() const { return state_.y; }

    Brush color(theme::ColorId color_id);
    Brush blendedColor(theme::ColorId color_from, theme::ColorId color_to, float t) {
      return color(color_from).interpolateWith(color(color_to), t);
    }

    float value(theme::ValueId value_id);
    std::vector<std::string> debugInfo() const;

    ImageAtlas* imageAtlas() { return &image_atlas_; }
    GradientAtlas* gradientAtlas() { return &gradient_atlas_; }

    State* state() { return &state_; }

  private:
    template<typename T>
    void addShape(T shape) {
      state_.current_region->shape_batcher_.addShape(std::move(shape), state_.blend_mode);
    }

    void fullRoundedRectangleBorder(float x, float y, float width, float height, float rounding,
                                    float thickness) {
      RoundedRectangle border(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height,
                              rounding);
      border.thickness = thickness;
      addShape(border);
    }

    void outerRoundedTriangleBorder(float a_x, float a_y, float b_x, float b_y, float c_x,
                                    float c_y, float rounding, float thickness) {
      float pad = rounding;
      float x = std::min(std::min(a_x, b_x), c_x) - pad;
      float width = std::max(std::max(a_x, b_x), c_x) - x + 2.0f * pad;
      float y = std::min(std::min(a_y, b_y), c_y) - pad;
      float height = std::max(std::max(a_y, b_y), c_y) - y + 2.0f * pad;

      float x1 = 2.0f * (a_x - x) / width - 1.0f;
      float y1 = 2.0f * (a_y - y) / height - 1.0f;
      float x2 = 2.0f * (b_x - x) / width - 1.0f;
      float y2 = 2.0f * (b_y - y) / height - 1.0f;
      float x3 = 2.0f * (c_x - x) / width - 1.0f;
      float y3 = 2.0f * (c_y - y) / height - 1.0f;

      addShape(Triangle(state_.clamp, state_.brush, state_.x + x, state_.y + y, width, height, x1,
                        y1, x2, y2, x3, y3, rounding, thickness + 1.0f));
    }

    bool tryDrawCollinearQuadratic(float a_x, float a_y, float b_x, float b_y, float c_x, float c_y,
                                   float thickness, float pixel_width = 1.0f) {
      static constexpr float kLinearThreshold = 0.01f;

      float collinear_distance_x = a_x - 2.0f * b_x + c_x;
      float collinear_distance_y = a_y - 2.0f * b_y + c_y;
      if (collinear_distance_x > kLinearThreshold || collinear_distance_x < -kLinearThreshold ||
          collinear_distance_y > kLinearThreshold || collinear_distance_y < -kLinearThreshold) {
        return false;
      }

      segment(a_x, a_y, c_x, c_y, thickness, true, pixel_width);
      return true;
    }

    Palette* palette_ = nullptr;
    float width_scale_ = 1.0f;
    float height_scale_ = 1.0f;
    float dpi_scale_ = 1.0f;
    double render_time_ = 0.0;
    double delta_time_ = 0.0;
    int render_frame_ = 0;

    std::vector<State> state_memory_;
    State state_;

    GradientAtlas gradient_atlas_;
    ImageAtlas image_atlas_;

    Region window_region_;
    Region default_region_;
    Layer composite_layer_;
    std::vector<std::unique_ptr<Layer>> intermediate_layers_;
    std::vector<Layer*> layers_;

    float refresh_rate_ = 0.0f;

    VISAGE_LEAK_CHECKER(Canvas)
  };
}
