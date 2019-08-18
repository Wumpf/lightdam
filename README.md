# lightdam
A DXR toy raytracer named after the light dams mentioned in Terry Prattchett's Colour of Magic


## todo & things to try

* write a proper readme
* scene data
    * don't store everything in upload heaps (was supposed only to be a convenience to get started)
    * drop vertex positions from vertex buffers used during tracing (redundant!)
* materials & lighting
    * textures
    * specular/more materials
    * support lights other than area lights [...]
    * sampling
        * better/configurable light sampling - biggest problem now is that every hit is sampling the same light samples, means longer convergence
        * fix color of area light surfaces
        * simple MIS for next event estimation & direct hits
    * other light transport algorithms
        * bidirectional path tracing
        * photon mapping
* video on how light arrives over time - this was the original motivation for the project!
* upload more scenes to repository
* ...
