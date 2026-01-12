#!/usr/bin/env python3
"""
Generate GeoJSON polygon files for scautoloc region-based depth constraints.

This script helps create GeoJSON files with defaultDepth and maxDepth attributes
that can be used by scautoloc's regionDepth feature.

Usage:
    python generate_depth_regions.py --output regions.geojson

The generated files should be placed in:
    $SEISCOMP_ROOT/share/spatial/vector/

Example configuration in scautoloc.cfg:
    autoloc.regionDepth.enable = true
    autoloc.regionDepth.regions = stable_craton, subduction_zone, volcanic_area
"""

import argparse
import json
import sys


def create_polygon_feature(name, coordinates, default_depth, max_depth, rank=1):
    """
    Create a GeoJSON Feature for a depth region.

    Args:
        name: Region name (used in scautoloc config)
        coordinates: List of [lon, lat] pairs forming a closed polygon
        default_depth: Default depth in km when depth cannot be resolved
        max_depth: Maximum allowed depth in km for this region
        rank: Priority rank (lower = higher priority)

    Returns:
        GeoJSON Feature dict
    """
    # Ensure polygon is closed
    if coordinates[0] != coordinates[-1]:
        coordinates = coordinates + [coordinates[0]]

    return {
        "type": "Feature",
        "properties": {
            "name": name,
            "rank": rank,
            "defaultDepth": str(default_depth),
            "maxDepth": str(max_depth)
        },
        "geometry": {
            "type": "Polygon",
            "coordinates": [coordinates]
        }
    }


def create_example_regions():
    """
    Create example depth regions for different tectonic settings.
    These are simplified examples - real regions should be based on
    actual tectonic boundaries.
    """
    regions = []

    # Example 1: Stable craton (shallow earthquakes only)
    # This is a simplified example polygon
    stable_craton = create_polygon_feature(
        name="stable_craton",
        coordinates=[
            [-100.0, 35.0],
            [-95.0, 35.0],
            [-95.0, 40.0],
            [-100.0, 40.0],
            [-100.0, 35.0]
        ],
        default_depth=10,
        max_depth=35,
        rank=1
    )
    regions.append(stable_craton)

    # Example 2: Subduction zone (deep earthquakes possible)
    subduction_zone = create_polygon_feature(
        name="subduction_zone",
        coordinates=[
            [-125.0, 40.0],
            [-120.0, 40.0],
            [-120.0, 50.0],
            [-125.0, 50.0],
            [-125.0, 40.0]
        ],
        default_depth=35,
        max_depth=700,
        rank=1
    )
    regions.append(subduction_zone)

    # Example 3: Volcanic area (very shallow)
    volcanic_area = create_polygon_feature(
        name="volcanic_area",
        coordinates=[
            [-122.5, 45.0],
            [-121.5, 45.0],
            [-121.5, 46.0],
            [-122.5, 46.0],
            [-122.5, 45.0]
        ],
        default_depth=5,
        max_depth=20,
        rank=1
    )
    regions.append(volcanic_area)

    # Example 4: Mid-ocean ridge (shallow)
    mid_ocean_ridge = create_polygon_feature(
        name="mid_ocean_ridge",
        coordinates=[
            [-45.0, 0.0],
            [-40.0, 0.0],
            [-40.0, 10.0],
            [-45.0, 10.0],
            [-45.0, 0.0]
        ],
        default_depth=10,
        max_depth=15,
        rank=1
    )
    regions.append(mid_ocean_ridge)

    return regions


def create_region_from_bounds(name, min_lon, max_lon, min_lat, max_lat,
                               default_depth, max_depth, rank=1):
    """
    Create a rectangular region from bounding box coordinates.

    Args:
        name: Region name
        min_lon, max_lon: Longitude bounds
        min_lat, max_lat: Latitude bounds
        default_depth: Default depth in km
        max_depth: Maximum depth in km
        rank: Priority rank

    Returns:
        GeoJSON Feature dict
    """
    coordinates = [
        [min_lon, min_lat],
        [max_lon, min_lat],
        [max_lon, max_lat],
        [min_lon, max_lat],
        [min_lon, min_lat]
    ]
    return create_polygon_feature(name, coordinates, default_depth, max_depth, rank)


def write_geojson(features, output_file):
    """Write features to a GeoJSON file."""
    geojson = {
        "type": "FeatureCollection",
        "features": features
    }

    with open(output_file, 'w') as f:
        json.dump(geojson, f, indent=2)

    print(f"Written {len(features)} regions to {output_file}")


def write_bna(features, output_file):
    """
    Write features to BNA format (alternative to GeoJSON).
    BNA format is also supported by SeisComP's GeoFeatureSet.
    """
    with open(output_file, 'w') as f:
        for feature in features:
            props = feature['properties']
            coords = feature['geometry']['coordinates'][0]

            # BNA header: "name","rank","attributes",num_points
            attrs = f"defaultDepth: {props['defaultDepth']}, maxDepth: {props['maxDepth']}"
            f.write(f'"{props["name"]}","rank {props["rank"]}","{attrs}",{len(coords)}\n')

            # Write coordinates (lon, lat)
            for coord in coords:
                f.write(f"{coord[0]},{coord[1]}\n")

    print(f"Written {len(features)} regions to {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate GeoJSON/BNA files for scautoloc depth regions',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate example regions as GeoJSON
  %(prog)s --output depth_regions.geojson --examples

  # Generate example regions as BNA
  %(prog)s --output depth_regions.bna --format bna --examples

  # Create a single rectangular region
  %(prog)s --output my_region.geojson \\
      --name "my_region" \\
      --bounds -120 -115 30 35 \\
      --default-depth 15 \\
      --max-depth 50

After generating, copy to SeisComP spatial data directory:
  cp depth_regions.geojson $SEISCOMP_ROOT/share/spatial/vector/

Then configure scautoloc:
  autoloc.regionDepth.enable = true
  autoloc.regionDepth.regions = stable_craton, subduction_zone, volcanic_area
        """
    )

    parser.add_argument('--output', '-o', required=True,
                        help='Output file path')
    parser.add_argument('--format', '-f', choices=['geojson', 'bna'],
                        default='geojson',
                        help='Output format (default: geojson)')
    parser.add_argument('--examples', '-e', action='store_true',
                        help='Generate example regions for different tectonic settings')

    # Options for creating a single region
    parser.add_argument('--name', '-n',
                        help='Region name')
    parser.add_argument('--bounds', '-b', nargs=4, type=float,
                        metavar=('MIN_LON', 'MAX_LON', 'MIN_LAT', 'MAX_LAT'),
                        help='Bounding box coordinates')
    parser.add_argument('--default-depth', '-d', type=float, default=10,
                        help='Default depth in km (default: 10)')
    parser.add_argument('--max-depth', '-m', type=float, default=100,
                        help='Maximum depth in km (default: 100)')
    parser.add_argument('--rank', '-r', type=int, default=1,
                        help='Priority rank (default: 1)')

    args = parser.parse_args()

    features = []

    if args.examples:
        features = create_example_regions()
    elif args.name and args.bounds:
        feature = create_region_from_bounds(
            args.name,
            args.bounds[0], args.bounds[1],
            args.bounds[2], args.bounds[3],
            args.default_depth, args.max_depth, args.rank
        )
        features.append(feature)
    else:
        parser.error("Either --examples or --name with --bounds must be specified")

    if args.format == 'geojson':
        write_geojson(features, args.output)
    else:
        write_bna(features, args.output)

    # Print configuration hint
    print("\nTo use these regions in scautoloc, add to scautoloc.cfg:")
    print("  autoloc.regionDepth.enable = true")
    print("  autoloc.regionDepth.regions = " + ", ".join(
        f['properties']['name'] for f in features))


if __name__ == '__main__':
    main()
