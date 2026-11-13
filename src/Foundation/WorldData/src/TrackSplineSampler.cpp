#include "rr/world/TrackSplineSampler.hpp"

#include "TrackFrameInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rr::world {
namespace {

constexpr float kEpsilon = 1.0e-6f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTangentStep = 1.0e-3f;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

bool finite_vec(rr::math::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float safe_length(rr::math::Vec3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

rr::math::Vec3 normalized(rr::math::Vec3 value, const char* context) {
    const float length = safe_length(value);
    if (!std::isfinite(length) || length <= kEpsilon) {
        fail(std::string(context) + ": degenerate vector");
    }
    return value / length;
}

float knot_interval(rr::math::Vec3 first, rr::math::Vec3 second) {
    const float length = safe_length(second - first);
    if (!std::isfinite(length) || length <= kEpsilon) {
        return 0.0f;
    }
    return std::sqrt(length); // centripetal alpha = 0.5
}

struct Segment {
    TrackPoint p0{};
    TrackPoint p1{};
    TrackPoint p2{};
    TrackPoint p3{};
    float t0{0.0f};
    float t1{0.0f};
    float t2{0.0f};
    float t3{0.0f};
};

rr::math::Vec3 evaluate_segment(const Segment& segment, float t) {
    const double u = static_cast<double>(segment.t1) +
                     static_cast<double>(segment.t2 - segment.t1) * static_cast<double>(t);
    const auto blend = [](const rr::math::Vec3& first,
                          const rr::math::Vec3& second,
                          double first_weight,
                          double second_weight) {
        return first * static_cast<float>(first_weight) +
               second * static_cast<float>(second_weight);
    };

    const auto a1 = blend(segment.p0.position, segment.p1.position,
                          (segment.t1 - u) / static_cast<double>(segment.t1 - segment.t0),
                          (u - segment.t0) / static_cast<double>(segment.t1 - segment.t0));
    const auto a2 = blend(segment.p1.position, segment.p2.position,
                          (segment.t2 - u) / static_cast<double>(segment.t2 - segment.t1),
                          (u - segment.t1) / static_cast<double>(segment.t2 - segment.t1));
    const auto a3 = blend(segment.p2.position, segment.p3.position,
                          (segment.t3 - u) / static_cast<double>(segment.t3 - segment.t2),
                          (u - segment.t2) / static_cast<double>(segment.t3 - segment.t2));
    const auto b1 = blend(a1, a2,
                          (segment.t2 - u) / static_cast<double>(segment.t2 - segment.t0),
                          (u - segment.t0) / static_cast<double>(segment.t2 - segment.t0));
    const auto b2 = blend(a2, a3,
                          (segment.t3 - u) / static_cast<double>(segment.t3 - segment.t1),
                          (u - segment.t1) / static_cast<double>(segment.t3 - segment.t1));
    return blend(b1, b2,
                 (segment.t2 - u) / static_cast<double>(segment.t2 - segment.t1),
                 (u - segment.t1) / static_cast<double>(segment.t2 - segment.t1));
}

rr::math::Vec3 evaluate_tangent(const Segment& segment, float t) {
    float before = t - kTangentStep;
    float after = t + kTangentStep;
    if (before < 0.0f) {
        before = 0.0f;
    }
    if (after > 1.0f) {
        after = 1.0f;
    }
    if (after - before <= kEpsilon) {
        fail("spline segment has zero tangent interval");
    }
    return normalized(evaluate_segment(segment, after) - evaluate_segment(segment, before),
                      "spline tangent");
}

Segment make_segment(const std::vector<TrackPoint>& points,
                     bool closed,
                     std::size_t index) {
    const std::size_t count = points.size();
    Segment segment;
    if (closed) {
        segment.p0 = points[(index + count - 1) % count];
        segment.p1 = points[index % count];
        segment.p2 = points[(index + 1) % count];
        segment.p3 = points[(index + 2) % count];
    } else {
        segment.p1 = points[index];
        segment.p2 = points[index + 1];
        segment.p0 = index > 0 ? points[index - 1]
                               : TrackPoint{segment.p1.position * 2.0f - segment.p2.position,
                                            segment.p1.road_width_m, segment.p1.banking_rad,
                                            segment.p1.surface_id};
        segment.p3 = index + 2 < count
                         ? points[index + 2]
                         : TrackPoint{segment.p2.position * 2.0f - segment.p1.position,
                                      segment.p2.road_width_m, segment.p2.banking_rad,
                                      segment.p2.surface_id};
    }

    segment.t0 = 0.0f;
    segment.t1 = segment.t0 + knot_interval(segment.p0.position, segment.p1.position);
    segment.t2 = segment.t1 + knot_interval(segment.p1.position, segment.p2.position);
    segment.t3 = segment.t2 + knot_interval(segment.p2.position, segment.p3.position);
    if (segment.t1 - segment.t0 <= kEpsilon ||
        segment.t2 - segment.t1 <= kEpsilon ||
        segment.t3 - segment.t2 <= kEpsilon) {
        fail("spline contains a zero-length or duplicate control segment");
    }
    return segment;
}

struct RawSample {
    rr::math::Vec3 position{};
    float distance_m{0.0f};
    std::size_t segment_index{0};
    float segment_t{0.0f};
};

struct SamplingState {
    const TrackSpline& spline;
    const TrackSamplingConfig& config;
    std::vector<RawSample> samples;
    std::size_t generated{0};
};

void append_adaptive(SamplingState& state,
                     const Segment& segment,
                     std::size_t segment_index,
                     float t0,
                     rr::math::Vec3 position0,
                     float t1,
                     rr::math::Vec3 position1,
                     std::size_t depth) {
    const float middle_t = (t0 + t1) * 0.5f;
    const rr::math::Vec3 middle_position = evaluate_segment(segment, middle_t);
    const float chord = safe_length(position1 - position0);
    const float sagitta = safe_length(middle_position - (position0 + position1) * 0.5f);
    const rr::math::Vec3 tangent0 = evaluate_tangent(segment, t0);
    const rr::math::Vec3 tangent1 = evaluate_tangent(segment, t1);
    const float cosine = std::clamp(glm::dot(tangent0, tangent1), -1.0f, 1.0f);
    const float tangent_angle = std::acos(cosine);

    const bool needs_subdivision =
        chord > state.spline.max_sample_spacing_m ||
        sagitta > state.spline.max_sagitta_error_m ||
        tangent_angle > state.config.max_tangent_angle_error_rad;

    if (needs_subdivision) {
        if (depth >= state.config.max_subdivision_depth) {
            fail("adaptive spline subdivision exceeded configured depth");
        }
        append_adaptive(state, segment, segment_index, t0, position0, middle_t, middle_position,
                        depth + 1);
        append_adaptive(state, segment, segment_index, middle_t, middle_position, t1, position1,
                        depth + 1);
        return;
    }

    ++state.generated;
    if (state.generated > state.config.max_samples) {
        fail("spline sampling exceeds configured vertex budget");
    }
    state.samples.push_back(RawSample{position1, 0.0f, segment_index, t1});
}

} // namespace

namespace detail {

using rr::math::Quat;
using rr::math::Vec3;

Quat minimal_rotation(Vec3 from, Vec3 to) {
    const float cosine = std::clamp(glm::dot(from, to), -1.0f, 1.0f);
    const Vec3 axis = glm::cross(from, to);
    const float sine = safe_length(axis);
    if (sine <= kEpsilon) {
        if (cosine >= 0.0f) {
            return Quat{1.0f, 0.0f, 0.0f, 0.0f};
        }

        // Exact/near-opposite tangents: use a deterministic perpendicular axis.
        // +X is the first candidate; if it is parallel to `from`, use +Z.
        Vec3 fallback = glm::cross(from, Vec3{1.0f, 0.0f, 0.0f});
        if (safe_length(fallback) <= kEpsilon) {
            fallback = glm::cross(from, Vec3{0.0f, 0.0f, 1.0f});
        }
        fallback = normalized(fallback, "opposite tangent fallback");
        return glm::angleAxis(kPi, fallback);
    }
    return glm::angleAxis(std::atan2(sine, cosine), axis / sine);
}

TrackFrame transport_frame(const TrackFrame& previous, Vec3 tangent) {
    TrackFrame frame;
    frame.tangent = normalized(tangent, "transport tangent");
    const Quat rotation = minimal_rotation(previous.tangent, frame.tangent);
    frame.right = rotation * previous.right;
    frame.up = rotation * previous.up;

    frame.right = frame.right - frame.tangent * glm::dot(frame.right, frame.tangent);
    frame.right = normalized(frame.right, "transported right");
    frame.up = normalized(glm::cross(frame.tangent, frame.right), "transported up");
    frame.right = normalized(glm::cross(frame.up, frame.tangent), "reorthonormalized right");
    return frame;
}

} // namespace detail

namespace {

rr::math::Vec3 rotate_about_tangent(rr::math::Vec3 vector,
                                    rr::math::Vec3 tangent,
                                    float angle) {
    const rr::math::Quat rotation = glm::angleAxis(angle, tangent);
    return normalized(rotation * vector, "banked frame vector");
}

detail::TrackFrame initial_frame(rr::math::Vec3 tangent) {
    detail::TrackFrame frame;
    frame.tangent = normalized(tangent, "initial tangent");
    rr::math::Vec3 reference{0.0f, 1.0f, 0.0f};
    if (std::fabs(glm::dot(frame.tangent, reference)) > 0.999f) {
        reference = rr::math::Vec3{0.0f, 0.0f, 1.0f};
    }
    frame.right = normalized(glm::cross(reference, frame.tangent), "initial right");
    frame.up = normalized(glm::cross(frame.tangent, frame.right), "initial up");
    return frame;
}

float seam_angle(rr::math::Vec3 first_right,
                 rr::math::Vec3 last_right,
                 rr::math::Vec3 tangent) {
    const float cosine = std::clamp(glm::dot(first_right, last_right), -1.0f, 1.0f);
    const float sine = glm::dot(glm::cross(first_right, last_right), tangent);
    return std::atan2(sine, cosine);
}

float orthonormality_error(const detail::TrackFrame& frame) {
    const float right_length = safe_length(frame.right);
    const float up_length = safe_length(frame.up);
    const float tangent_length = safe_length(frame.tangent);
    return std::max({std::fabs(right_length - 1.0f),
                     std::fabs(up_length - 1.0f),
                     std::fabs(tangent_length - 1.0f),
                     std::fabs(glm::dot(frame.right, frame.up)),
                     std::fabs(glm::dot(frame.right, frame.tangent)),
                     std::fabs(glm::dot(frame.up, frame.tangent))});
}

} // namespace

float ArcLengthTable::total_length_m() const noexcept {
    return entries_.empty() ? 0.0f : entries_.back().distance_m;
}

const std::vector<ArcLengthEntry>& ArcLengthTable::entries() const noexcept {
    return entries_;
}

bool ArcLengthTable::parameter_at_distance(float distance_m,
                                           std::size_t& segment_index,
                                           float& segment_t) const noexcept {
    if (entries_.empty() || !std::isfinite(distance_m) || distance_m < 0.0f) {
        return false;
    }
    if (distance_m <= 0.0f) {
        segment_index = entries_.front().segment_index;
        segment_t = entries_.front().segment_t;
        return true;
    }
    if (distance_m >= entries_.back().distance_m) {
        segment_index = entries_.back().segment_index;
        segment_t = entries_.back().segment_t;
        return true;
    }

    const auto upper = std::lower_bound(
        entries_.begin(), entries_.end(), distance_m,
        [](const ArcLengthEntry& entry, float distance) {
            return entry.distance_m < distance;
        });
    if (upper == entries_.begin()) {
        segment_index = upper->segment_index;
        segment_t = upper->segment_t;
        return true;
    }
    const auto lower = upper - 1;
    const float span = upper->distance_m - lower->distance_m;
    const float fraction = span > kEpsilon ? (distance_m - lower->distance_m) / span : 0.0f;
    if (upper->segment_index == lower->segment_index) {
        segment_index = lower->segment_index;
        segment_t = lower->segment_t + (upper->segment_t - lower->segment_t) * fraction;
    } else {
        segment_index = upper->segment_index;
        segment_t = upper->segment_t * fraction;
    }
    return true;
}

std::optional<SampledTrack> TrackSplineSampler::sample(const TrackSpline& spline,
                                                       const TrackSamplingConfig& config,
                                                       std::string& error) {
    try {
        if (spline.points.size() < 2) {
            fail("spline must have at least two control points");
        }
        if (spline.closed && spline.points.size() < 3) {
            fail("closed spline must have at least three control points");
        }
        if (config.max_samples < 4) {
            fail("max_samples must be at least 4");
        }
        if (!std::isfinite(spline.texture_repeat_m) || spline.texture_repeat_m <= 0.0f) {
            fail("texture_repeat_m must be finite and positive");
        }
        if (!std::isfinite(spline.max_sample_spacing_m) || spline.max_sample_spacing_m <= 0.0f) {
            fail("max_sample_spacing_m must be finite and positive");
        }
        if (!std::isfinite(spline.max_sagitta_error_m) || spline.max_sagitta_error_m <= 0.0f) {
            fail("max_sagitta_error_m must be finite and positive");
        }
        if (!std::isfinite(config.max_tangent_angle_error_rad) ||
            config.max_tangent_angle_error_rad <= 0.0f) {
            fail("max_tangent_angle_error_rad must be finite and positive");
        }

        std::vector<TrackPoint> points = spline.points;
        if (spline.closed && points.size() >= 3 &&
            safe_length(points.front().position - points.back().position) <= kEpsilon) {
            points.pop_back();
        }
        if (spline.closed && points.size() < 3) {
            fail("closed spline has fewer than three unique control points");
        }
        if (!spline.closed && points.size() < 2) {
            fail("open spline has fewer than two control points");
        }
        for (const TrackPoint& point : points) {
            if (!finite_vec(point.position) || !std::isfinite(point.road_width_m) ||
                !std::isfinite(point.banking_rad) || point.surface_id.empty()) {
                fail("spline control point contains invalid or non-finite data");
            }
        }

        const std::size_t segment_count = spline.closed ? points.size() : points.size() - 1;
        std::vector<Segment> segments;
        segments.reserve(segment_count);
        for (std::size_t index = 0; index < segment_count; ++index) {
            segments.push_back(make_segment(points, spline.closed, index));
        }

        SamplingState state{spline, config, {}, 0};
        for (std::size_t segment_index = 0; segment_index < segments.size(); ++segment_index) {
            const Segment& segment = segments[segment_index];
            const rr::math::Vec3 start_position = evaluate_segment(segment, 0.0f);
            if (segment_index == 0) {
                ++state.generated;
                state.samples.push_back(RawSample{start_position, 0.0f, 0, 0.0f});
            }
            append_adaptive(state, segment, segment_index, 0.0f, start_position, 1.0f,
                            evaluate_segment(segment, 1.0f), 0);
        }

        if (state.samples.size() < 2) {
            fail("spline produced fewer than two samples");
        }

        float distance = 0.0f;
        state.samples[0].distance_m = 0.0f;
        for (std::size_t index = 1; index < state.samples.size(); ++index) {
            distance += safe_length(state.samples[index].position - state.samples[index - 1].position);
            state.samples[index].distance_m = distance;
        }
        if (!std::isfinite(distance) || distance <= kEpsilon) {
            fail("spline total length is degenerate");
        }

        SampledTrack result;
        result.closed = spline.closed;
        result.total_length_m = distance;

        result.arc_length.entries_.reserve(state.samples.size());
        for (const RawSample& sample : state.samples) {
            result.arc_length.entries_.push_back(
                ArcLengthEntry{sample.distance_m, sample.segment_index, sample.segment_t});
        }

        // Build base RMF for every raw sample, including the closing sample.
        std::vector<detail::TrackFrame> base_frames;
        base_frames.reserve(state.samples.size());
        detail::TrackFrame frame = initial_frame(evaluate_tangent(
            segments[state.samples.front().segment_index], state.samples.front().segment_t));
        base_frames.push_back(frame);
        for (std::size_t index = 1; index < state.samples.size(); ++index) {
            const RawSample& sample = state.samples[index];
            const rr::math::Vec3 tangent =
                evaluate_tangent(segments[sample.segment_index], sample.segment_t);
            frame = detail::transport_frame(frame, tangent);
            base_frames.push_back(frame);
        }

        float residual = 0.0f;
        if (spline.closed) {
            const RawSample& first = state.samples.front();
            const RawSample& last = state.samples.back();
            result.seam_position_error_m = safe_length(last.position - first.position);
            const rr::math::Vec3 first_tangent =
                evaluate_tangent(segments[first.segment_index], first.segment_t);
            const rr::math::Vec3 last_tangent =
                evaluate_tangent(segments[last.segment_index], last.segment_t);
            result.seam_tangent_error_rad = std::acos(std::clamp(
                glm::dot(first_tangent, last_tangent), -1.0f, 1.0f));
            residual = seam_angle(base_frames.front().right, base_frames.back().right,
                                  first_tangent);
            result.holonomy_residual_rad = residual;

            for (std::size_t index = 0; index < state.samples.size(); ++index) {
                const float fraction = state.samples[index].distance_m / distance;
                base_frames[index].right = rotate_about_tangent(
                    base_frames[index].right, base_frames[index].tangent,
                    -residual * fraction);
                base_frames[index].up = normalized(
                    glm::cross(base_frames[index].tangent, base_frames[index].right),
                    "holonomy-corrected up");
                base_frames[index].right = normalized(
                    glm::cross(base_frames[index].up, base_frames[index].tangent),
                    "holonomy-corrected right");
                base_frames[index].up = normalized(
                    glm::cross(base_frames[index].tangent, base_frames[index].right),
                    "holonomy-corrected up");
            }

            const float corrected_residual = seam_angle(
                base_frames.front().right, base_frames.back().right,
                evaluate_tangent(segments[first.segment_index], first.segment_t));
            result.seam_angular_error_rad = std::fabs(corrected_residual);
        }

        const std::size_t output_count =
            spline.closed ? state.samples.size() - 1 : state.samples.size();
        result.samples.reserve(output_count);
        for (std::size_t index = 0; index < output_count; ++index) {
            const RawSample& raw = state.samples[index];
            const std::size_t segment_index = raw.segment_index;
            const TrackPoint& first = points[segment_index % points.size()];
            const TrackPoint& second = spline.closed
                                           ? points[(segment_index + 1) % points.size()]
                                           : points[segment_index + 1];

            const float width = first.road_width_m +
                                (second.road_width_m - first.road_width_m) * raw.segment_t;
            const float banking = first.banking_rad +
                                  (second.banking_rad - first.banking_rad) * raw.segment_t;

            TrackSample sample;
            sample.position = raw.position;
            sample.tangent = base_frames[index].tangent;
            sample.right = base_frames[index].right * std::cos(banking) +
                           base_frames[index].up * std::sin(banking);
            sample.up = -base_frames[index].right * std::sin(banking) +
                        base_frames[index].up * std::cos(banking);
            sample.right = normalized(sample.right, "banked right");
            sample.up = normalized(sample.up, "banked up");
            sample.distance_m = raw.distance_m;
            sample.uv_v = raw.distance_m / spline.texture_repeat_m;
            sample.road_width_m = width;
            sample.banking_rad = banking;
            sample.surface_id = first.surface_id;
            sample.segment_index = segment_index;
            sample.segment_t = raw.segment_t;
            sample.holonomy_correction_rad =
                spline.closed ? -result.holonomy_residual_rad *
                                     (raw.distance_m / result.total_length_m)
                               : 0.0f;

            const detail::TrackFrame check_frame{sample.tangent, sample.right, sample.up};
            result.max_orthonormality_error =
                std::max(result.max_orthonormality_error, orthonormality_error(check_frame));
            result.samples.push_back(std::move(sample));
        }

        if (result.max_orthonormality_error > config.orthonormal_tolerance) {
            fail("generated frames exceed configured orthonormality tolerance");
        }
        if (spline.closed &&
            (result.seam_position_error_m > 1.0e-3f ||
             result.seam_tangent_error_rad > 1.0e-2f ||
             result.seam_angular_error_rad > 1.0e-3f)) {
            fail("closed spline seam did not close within tolerance");
        }

        error.clear();
        return result;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

} // namespace rr::world
