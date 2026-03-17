# Propuesta de sistema de archivos

El sistema tendrá un directorio raíz desde el cual se podrán crear otros directorios y guardar archivos. La estructura de los directorios se manejará de forma jerárquica, donde cada carpeta puede contener otras carpetas o archivos.

Para organizar los archivos dentro de cada directorio se utilizará una lista enlazada. De esta forma cada directorio tendrá una lista con los archivos y subdirectorios que contiene. Esto permite recorrer fácilmente los elementos de un directorio cuando se quiera buscar o mostrar su contenido.

Cada archivo tendrá algunos metadatos básicos que permitan identificarlo dentro del sistema. Entre estos se incluyen el nombre del archivo, su tamaño, la fecha de creación, la fecha de última modificación y los permisos de acceso. También se guardará información sobre en qué parte del disco se encuentran los datos del archivo.

El sistema permitirá realizar operaciones básicas como crear archivos, escribir información en ellos, leer archivos existentes y eliminarlos. También se podrán crear y eliminar directorios para organizar mejor la información.