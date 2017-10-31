# ondra_shared
Just list of sources (mostly headers) shared in many projects


# To add new source here

 1. Only header is the best. Header with tcc file is also acceptable. Adding .h + .cpp only if there is no other way.
 2. #pragma once is forbidden. The include guards are welcomed. The every include guard should contain namespace "__ONDRA_SHARED", followed by name of the source and many random characters 
 
```
 #ifndef __ONDRA_SHARED_EXAMPLE_H_9832781HHJINBO_
 #define __ONDRA_SHARED_EXAMPLE_H_9832781HHJINBO_
 
 #endif
```
 3. every identifier must be member of ondra_shared namespace. 
 4. internal identifiers and helpers should be members of _intr or _details
 5. templates are welcomed
 
 
 # to include into project
 
 1. use submodules
 2. if used in libraries, put it into subdirectory of the src folder
 3. if used in large project, put it into separate directory
 
 # to use in code
 
 1. it is better to import each identifier into your namespace. Do not referer the identifier directly
 2. it is recomennded to define aliases in one shared file
 3. do not use ```using namespace ondra_shared```;
 
