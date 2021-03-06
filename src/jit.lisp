(in-package :hylas)
(annot:enable-annot-syntax)

@document "Low-level code generation and LLVM interface."

@doc "This function destructively modifies the `code` object that is passed as
its argument, so it should only be used within a call to `append-entry`. It
returns a copy of the (updated) object, so it can be queried as usual for things
like the last register value."
(defun emit-code (form code &optional &key (in-lambda nil))
  (cond
    ((null form)
      (emit-unit))
    ((atom form)
      (cond
        ((eq '|true| form)
          (append-entry code
            (assign-res (int 1) (constant (int 1) "true"))))
        ((eq '|false| form)
          (append-entry code
            (assign-res (int 1) (constant (int 1) "false"))))
        ((integerp form)
          (append-entry code
            (assign-res (int (min-width form))
              (constant (int (min-width form)) form))))
        ((floatp form)
          (append-entry code
            (assign-res +double+ (constant +double+ form))))
        ((stringp form)
          (let ((bytes (trivial-utf-8:string-to-utf-8-bytes form)))
            (append-entry
              (append-toplevel code
                (assign
                  (new-string code)
                  (format nil "global [~A x i8] c[~{\\~A~}\\00]" (length bytes)
                    (loop for char across bytes collecting
                      (write-to-string char :base 16)))))
              (assign (res code +cstr+) (memload +cstr+ (current-string code))))))
        ((symbolp form)
          (multiple-value-bind (var pos) (lookup form code)
            (if var
                (progn
                  ;; If we're in a lambda, check whether the symbol comes
                  ;; from a lexical context other than the local or global
                  ;; ones
                  (let ((latest-lambda-pos
                          (loop for i from (1- (length (stack code))) to 0
                            if (eql :lambda (context (nth i (stack code))))
                            return i)))
                    (when latest-lambda-pos
                      ;; So we're in a lambda. Append the (symbol pos) pair to the
                      ;; latest lambda context
                      (push (list form code) (car (last (lambda-contexts code))))))
                  (append-entry code
                    (assign-res (var-type var)
                      (memload (var-type var) (emit-var form var pos)))))
                (raise form "Unbound symbol"))))))
    (t
      (let ((fn (car form)))
        ;; Input is a list
        (aif (operator? fn code)
          (funcall it (cdr form) code)
          ;;No? Well, user-defined function then
          (aif (callfn fn (cdr form) code)
               it
               ;; Not that? Then it's part of the normal core
               (aif (integer-constructor? fn)
                    (construct-integer it (cdr form) code)
                    (aif (float-constructor? fn)
                         (construct-float it (cdr form) code)
                         (aif (core? fn code)
                           (funcall it (cdr form))
                           ;; Since everything above failed, signal an error
                           (raise form "No such function"))))))))))

@doc "Takes a form. Produces global IR"
(defun compile-code (form code)
  (let ((out (emit-code form code)))
    (list out
          (format nil "~{~A~&~}~&define ~A @entry(){~&~{    ~A~&~}~&    ret ~A ~A~&}"
                  (toplevel out) (res-type out) (entry out) (res-type out) (res out)))))

(defmacro with-preserved-case (&rest code)
  `(unwind-protect
     (progn
       (setf (readtable-case *readtable*) :preserve)
       ,@code)
     (setf (readtable-case *readtable*) :upcase)))

(defun eval-string (str)
  (with-preserved-case
    (compile-code (safe-read-from-string str) initial-code)))

;;; Interface to the C++ backend

(cffi:load-foreign-library
  (namestring
    (make-pathname :name "libhylas"
                   :type #+unix "so" #+darwin "dylib" #+windows "dll"
                   :defaults (asdf::component-relative-pathname
                               (asdf:find-system :hylas)))))

(cffi:defctype c-code :pointer)

(cffi:defcfun "backend_init" c-code)

(cffi:defcfun "jit_ir" c-code (code c-code) (ir :string))

(cffi:defcfun "get_error" :string (code c-code))

(cffi:defcfun (link-lib "link") :char (lib :string))

(cffi:defcfun "nconcurrent" :int)

(cffi:defcfun "run_entry" :string (code c-code))

(cffi:defcfun "delete_code" :char (code c-code))

(cffi:defcfun "repeat" :string (str :string))

(defun jit (c-code ir)
  (cffi:with-foreign-string (ir ir)
    (let ((result (jit-ir c-code ir)))
      (aif (get-error result) it result))))

(defun link (lib)
  (cffi:with-foreign-string (lib lib)
    (link-lib lib)))
