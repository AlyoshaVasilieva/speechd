<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
 <!ENTITY docbook.dsl SYSTEM 
	  "/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl" 
	  CDATA DSSSL>
]>

<style-specification id="my-docbook-html" use="docbook">

 ;; Generate HTML 4.0
(define %html40% #t)

;; adres��, kam se maj� ukl�dat vygenerovan� HTML soubory
(define %output-dir% "html")

;; m� se pou��vat v��e uveden� adres��
(define use-output-dir #t)

;; jm�no hlavn� str�nky
(define %root-filename% "index")

;; maj� se jm�na odvozovat z hodnoty atributu ID
(define %use-id-as-filename% #t)

;; p��pona pro HTML soubory
(define %html-ext% ".html")

;; do hlavi�ky str�nky p�id�me informaci o pou�it�m k�dov�n�
(define %html-header-tags%
  '(("META"
     ("HTTP-EQUIV" "Content-Type")
     ("CONTENT" "text/html; charset=ISO-8859-1"))))

;; kde se maj� geenrovat naviga�n� odkazy
(define %header-navigation% #t)
(define %footer-navigation% #t)

;; Name of the stylesheet to use
;(define %stylesheet% "speechd.css")
;; The type of the stylesheet to use
;(define %stylesheet-type% "text/css")

</style-specification>

<external-specification id="docbook" document="docbook.dsl">