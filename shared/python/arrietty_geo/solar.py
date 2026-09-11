"""Solar geometry adapted from Secret-World 0.1.0 (MIT), NOAA formula."""
from datetime import timezone
import math

def position(instant, latitude, longitude):
    """Return apparent solar centre and ENU direction; azimuth clockwise north."""
    if instant.tzinfo is None or instant.utcoffset() is None:
        raise ValueError('A timezone-aware datetime is required')
    if not -90 <= latitude <= 90 or not -180 <= longitude <= 180:
        raise ValueError('Invalid geographic coordinates')
    utc = instant.astimezone(timezone.utc)
    t = (utc.timestamp()/86400 + 2440587.5 - 2451545)/36525
    rad, deg = math.radians, math.degrees
    mean_long = (280.46646+t*(36000.76983+.0003032*t)) % 360
    anomaly = 357.52911+t*(35999.05029-.0001537*t)
    eccentricity = .016708634-t*(.000042037+.0000001267*t)
    centre = (math.sin(rad(anomaly))*(1.914602-t*(.004817+.000014*t))+
              math.sin(rad(2*anomaly))*(.019993-.000101*t)+math.sin(rad(3*anomaly))*.000289)
    omega = 125.04-1934.136*t
    longitude_sun = rad(mean_long+centre-.00569-.00478*math.sin(rad(omega)))
    obliquity = rad(23+(26+(21.448-t*(46.815+t*(.00059-t*.001813)))/60)/60+
                     .00256*math.cos(rad(omega)))
    decl = math.asin(math.sin(obliquity)*math.sin(longitude_sun))
    y = math.tan(obliquity/2)**2
    l, m = rad(mean_long), rad(anomaly)
    eqtime = 4*deg(y*math.sin(2*l)-2*eccentricity*math.sin(m)+
                  4*eccentricity*y*math.sin(m)*math.cos(2*l)-
                  .5*y*y*math.sin(4*l)-1.25*eccentricity**2*math.sin(2*m))
    minutes = utc.hour*60+utc.minute+utc.second/60+utc.microsecond/60000000
    hour_angle = rad(((minutes+eqtime+4*longitude)%1440)/4-180)
    lat = rad(latitude)
    sin_alt = math.sin(lat)*math.sin(decl)+math.cos(lat)*math.cos(decl)*math.cos(hour_angle)
    geometric = deg(math.asin(max(-1,min(1,sin_alt))))
    azimuth = (deg(math.atan2(math.sin(hour_angle),
               math.cos(hour_angle)*math.sin(lat)-math.tan(decl)*math.cos(lat)))+180)%360
    if geometric > 85:
        refraction = 0
    elif geometric > 5:
        tangent = math.tan(rad(geometric))
        refraction = (58.1/tangent-.07/tangent**3+.000086/tangent**5)/3600
    elif geometric > -.575:
        h = geometric
        refraction = (1735+h*(-518.2+h*(103.4+h*(-12.79+h*.711))))/3600
    else:
        refraction = -20.774/math.tan(rad(geometric))/3600
    apparent = geometric+refraction
    a, e = rad(azimuth),rad(apparent)
    return {'azimuth_degrees':azimuth,'elevation_degrees':apparent,
            'geometric_elevation_degrees':geometric,
            'direction_enu':(math.sin(a)*math.cos(e),math.cos(a)*math.cos(e),math.sin(e)),
            'utc':utc.isoformat(),'local':instant.isoformat()}
