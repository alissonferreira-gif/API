#pragma once

#include "idatabase.hpp"
#include <cmath>
#include <string>
#include <format>
#include <optional>
#include <limits>

class GeoLocation {
public:
    explicit GeoLocation(IDatabase& db) : db_(db) {}

    struct NearestResult {
        CollectionPoint point;
        double          distance_km;
        std::string     maps_link;
        std::string     formatted_message;
    };

    [[nodiscard]] std::optional<NearestResult> find_nearest(
        double user_lat, double user_lng) const
    {
        auto points = db_.get_collection_points("");
        if (points.empty()) return std::nullopt;

        const CollectionPoint* nearest = nullptr;
        double min_dist = std::numeric_limits<double>::max();

        for (const auto& p : points) {
            if (p.lat == 0.0 && p.lng == 0.0) continue;
            double d = haversine(user_lat, user_lng, p.lat, p.lng);
            if (d < min_dist) {
                min_dist = d;
                nearest  = &p;
            }
        }

        if (!nearest) return std::nullopt;

        NearestResult r;
        r.point       = *nearest;
        r.distance_km = min_dist;
        r.maps_link   = std::format(
            "https://maps.google.com/?q={},{}", nearest->lat, nearest->lng);

        r.formatted_message = std::format(
            "📍 *Ponto de coleta mais próximo:*\n\n"
            "🏛️ *{}*\n"
            "📮 {}\n"
            "🏘️ Bairro: {}\n"
            "📏 Distância: {:.1f} km\n\n"
            "🗺️ Rota: {}\n\n"
            "Nos vemos lá! +50 pontos ao registrar sua doação 💙",
            nearest->name, nearest->address,
            nearest->neighborhood, min_dist, r.maps_link);

        return r;
    }

    static bool parse_location(const std::string& json_body,
                                double& lat, double& lng)
    {
        auto find_val = [&](const std::string& key) -> std::optional<double> {
            auto pos = json_body.find("\"" + key + "\"");
            if (pos == std::string::npos) return std::nullopt;
            pos = json_body.find(':', pos);
            if (pos == std::string::npos) return std::nullopt;
            ++pos;
            while (pos < json_body.size() && json_body[pos] == ' ') ++pos;
            try { return std::stod(json_body.substr(pos)); }
            catch (...) { return std::nullopt; }
        };

        auto la = find_val("lat");
        auto ln = find_val("lng");
        if (!la || !ln) return false;
        lat = *la;
        lng = *ln;
        return true;
    }

private:
    IDatabase& db_;

    static double haversine(double lat1, double lng1,
                             double lat2, double lng2) noexcept
    {
        constexpr double R = 6371.0; // raio da Terra em km
        const double dlat = deg2rad(lat2 - lat1);
        const double dlng = deg2rad(lng2 - lng1);
        const double a = std::sin(dlat/2)*std::sin(dlat/2)
                       + std::cos(deg2rad(lat1)) * std::cos(deg2rad(lat2))
                       * std::sin(dlng/2)*std::sin(dlng/2);
        return R * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0-a));
    }

    static constexpr double deg2rad(double deg) noexcept {
        return deg * (3.14159265358979323846 / 180.0);
    }
};
