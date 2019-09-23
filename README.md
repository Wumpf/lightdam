# lightdam

A DXR toy raytracer named after the light dams mentioned in Terry Prattchett's Colour of Magic.

## Scenes

Uses PBRT scene format, but supports only a very limited subset. Console output generously informs about shortcomings. Where supported, it tries to stick closely to the spec though.

## todo & things to try

* write a proper readme
* denoiser
* materials & lighting
    * more materials
        * microfacet/lambert mix ("substrate")
    * support lights other than area lights [...]
    * sampling
        * simple MIS for next event estimation & direct hits
    * other light transport algorithms?
        * bidirectional path tracing
        * photon mapping
* upload more scenes to repository
* russian roulette per warp (can I query?) instead of per ray for higher efficiency
* ...
